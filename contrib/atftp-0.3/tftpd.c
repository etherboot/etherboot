/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftpd.c
 *    main server file
 *
 * $Id$
 *
 * Copyright (c) 2000 Jean-Pierre Lefebvre <helix@step.polymtl.ca>
 *                and Remi Lefebvre <remi@debian.org>
 *
 * atftp is free software; you can redistribute them and/or modify them
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h> 
#include <pthread.h>
#include <string.h>
#include "tftpd.h"
#include "tftp_io.h"
#include "tftp_def.h"
#include "logger.h"
#include "options.h"
#include "stats.h"

/*
 * Global variables set by main when starting. Read-only for threads
 */
int tftpd_max_thread = 100;     /* number of concurent thread allowed */
int tftpd_timeout = 300;        /* number of second of inactivity
                               before exiting */
char directory[MAXLEN] = "/tftpboot/";
int retry_timeout = S_TIMEOUT;

/*
 * "logging level" is the maximum error level that will get logged.
 *  This can be increased with the -v switch.
 */
int logging_level = LOG_NOTICE;
char *log_file = NULL;
 
/*
 * We need a lock on stdin from the time we notice fresh data coming from
 * stdin to the time the freshly created server thread as read it.
 */
pthread_mutex_t stdin_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Function defined in this file
 */
void *tftpd_receive_request(void *);
void signal_handler(int signal);
int tftpd_cmd_line_options(int argc, char **argv);
void tftpd_print_options(void);
void tftpd_usage(void);

/*
 * Main thread. Do required initialisation and then go through a loop
 * listening for client requests. When a request arrives, we allocate
 * memory for a thread data structure and start a thread to serve the
 * new client. If theres no activity for more than 'tftpd_timeout'
 * seconds, we exit and tftpd must be respawned by inetd.
 */
int main(int argc, char **argv)
{
     fd_set rfds;             /* for select */
     struct timeval tv;       /* for select */
     int run = 1;             /* while (run) loop */
     struct thread_data *new; /* for allocation of new thread_data */
     
     /* start collecting stats */
     stats_start();

     /*
      * Parse command line options. We parse before verifying
      * if we are running on a tty or not to make it possible to
      * verify the command line arguments
      */
     if (tftpd_cmd_line_options(argc, argv) == ERR)
          exit(1);

     /*
      * Can't be started from the prompt. Must be called by the
      * inetd superserver.
      */
     if (isatty(0))
     {
          tftpd_usage();
          exit(1);
     }
     
     /* Register signal handler. */
     signal(SIGINT, signal_handler);

     /* Using syslog facilties through a wrapper. */
     open_logger("tftpd", log_file, logging_level);
     logger(LOG_NOTICE, "Trivial FTP server started (%s)", VERSION);

     /* print summary of options */
     tftpd_print_options();

     /* Wait for read or write request and exit if timeout. */
     while (run)
     {
          /*
           * inetd dups the socket file descriptor to 0, 1 and 2 so we can
           * use any of those as the socket fd. We use 0. stdout and stderr
           * may not be used to print messages.
           */
          FD_ZERO(&rfds);
          FD_SET(0, &rfds);
          tv.tv_sec = tftpd_timeout;
          tv.tv_usec = 0;

          /* We need to lock stdin, and release it when the thread
             is done reading the request. */
          pthread_mutex_lock(&stdin_mutex);
          
          /* A timeout of 0 is interpreted as infinity */
          if (tftpd_timeout == 0)
               select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
          else
               select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
          
          if (FD_ISSET(0, &rfds))
          {
               /* Allocate memory for thread_data structure. */
               if ((new = malloc(sizeof(struct thread_data))) == NULL)
               {
                    logger(LOG_ERR, "%s: %d: Memory allocation failed",
                           __FILE__, __LINE__);
                    exit(1);
               }

               /*
                * Initialisation of thread_data structure.
                */
               pthread_mutex_init(&new->opt_mutex, NULL);
               pthread_mutex_init(&new->client_mutex, NULL);
               new->sockfd = 0;

               /* Allocate data buffer for tftp transfer. */
               if ((new->data_buffer = malloc((size_t)SEGSIZE + 4)) == NULL)
               {
                    logger(LOG_ERR, "%s: %d: Memory allocation failed",
                           __FILE__, __LINE__);
                    exit(1);
               }
               new->data_buffer_size = SEGSIZE + 4;

               /* Allocate ack buffer for tftp acknowledge. */
               if ((new->ack_buffer = malloc((size_t)SEGSIZE + 4)) == NULL)
               {
                    logger(LOG_ERR, "%s: %d: Memory allocation failed",
                           __FILE__, __LINE__);
                    exit(1);
               }
               new->ack_buffer_size = SEGSIZE + 4;               

               /* Allocate memory for tftp option structure. */
               if ((new->tftp_options = 
                    malloc(sizeof(tftp_default_options))) == NULL)
               {
                    logger(LOG_ERR, "%s: %d: Memory allocation failed",
                           __FILE__, __LINE__);
                    exit(1);
               }

               /* Copy default options. */
               memcpy(new->tftp_options, tftp_default_options,
                      sizeof(tftp_default_options));

               /* default timeout */
               new->timeout = retry_timeout;

               /* Allocate memory for a client structure. */
               if ((new->client_info =
                    calloc(1, sizeof(struct client_info))) == NULL)
               {
                    logger(LOG_ERR, "%s: %d: Memory allocation failed",
                           __FILE__, __LINE__);
                    exit(1);
               }
               new->client_info->master_client = 1;
               new->client_info->next = NULL;
               
               /* Start a new server thread. */
               pthread_create(&new->tid, NULL, tftpd_receive_request,
                              (void *)new);
          }
          else
          {
               pthread_mutex_unlock(&stdin_mutex);
               if (tftpd_list_num_of_thread() == 0)
               {
                    run = 0;
                    logger(LOG_NOTICE,
                           "Terminating after timeout of %d seconds",
                           tftpd_timeout);
               }
          }
     }

     close(0);
     close(1);
     close(2);

     /* stop collecting stats and print them*/
     stats_end();
     stats_print();
     
     /* some cleaning */
     if (log_file)
          free(log_file);

     logger(LOG_NOTICE, "Main thread exiting");
     close_logger();
     exit(0);
}

/* 
 * This function handles the initial connection with a client. It reads
 * the request from stdin and then release the stdin_mutex, so the main
 * thread may listen for new clients. After that, we process options and
 * call the sending or receiving function.
 *
 * arg is a thread_data structure pointer for that thread.
 */
void *tftpd_receive_request(void *arg)
{
     struct thread_data *data = (struct thread_data *)arg;
     
     int retval;                /* hold return value for testing */
     int data_size;             /* returned size by recvfrom */
     char string[MAXLEN];       /* hold the string we pass to the logger */
     int num_of_threads;
     int abort = 0;             /* 1 if we need to abort because the maximum
                                   number of threads have been reached*/
     socklen_t len = sizeof(struct sockaddr);

     /* Detach ourself. That way the main thread does not have to
      * wait for us with pthread_join. */
     pthread_detach(pthread_self());

     /* Verify the number of threads */
     if ((num_of_threads = tftpd_list_num_of_thread()) >= tftpd_max_thread)
          abort = 1;

     /* Read the first packet from stdin. */
     data_size = data->data_buffer_size;     
     retval = tftp_get_packet(0, &data->client_info->client, data->timeout,
                              &data_size, data->data_buffer);

     /* Add this new thread structure to the list. */
     if (!abort)
          stats_new_thread(tftpd_list_add(data));

     /* now unlock stdin */
     pthread_mutex_unlock(&stdin_mutex);


     /* if the maximum number of thread is reached, too bad we abort. */
     if (abort)
     {
          stats_abort_locked();
          logger(LOG_INFO, "Maximum number of threads reached: %d", num_of_threads);
     }
     else
     {
          /* open a socket for client communication */
          data->sockfd = socket(PF_INET, SOCK_DGRAM, 0);
          /* bind the socket to the interface */
          getsockname(0, (struct sockaddr *)&(data->sa_in), &len);
          data->sa_in.sin_port = 0;
          bind(data->sockfd, (struct sockaddr *)&(data->sa_in), len);
          getsockname(data->sockfd, (struct sockaddr *)&(data->sa_in), &len);
          connect(data->sockfd, (struct sockaddr *)&data->client_info->client,
                  sizeof(data->client_info->client));

          /* read options from request */
          pthread_mutex_lock(&data->opt_mutex);
          opt_parse_request(data->data_buffer, data_size,
                            data->tftp_options);
          pthread_mutex_unlock(&data->opt_mutex);
          
          opt_request_to_string(data->tftp_options, string, MAXLEN);
          
          /* Analyse the request. */
          switch (retval)
          {
          case GET_RRQ:
               logger(LOG_NOTICE, "Serving %s to %s",
                      data->tftp_options[OPT_FILENAME].value,
                      inet_ntoa(data->client_info->client.sin_addr));
               logger(LOG_DEBUG, "reveived RRQ <%s>", string);
               if (tftpd_send_file(data) == OK)
                    stats_send_locked();
               else
                    stats_err_locked();
               break;
          case GET_WRQ:
               logger(LOG_NOTICE, "Fetching from %s to %s",
                      inet_ntoa(data->client_info->client.sin_addr),
                      data->tftp_options[OPT_FILENAME].value);
               logger(LOG_DEBUG, "received WRQ <%s>", string);
               if (tftpd_receive_file(data) == OK)
                    stats_recv_locked();
               else
                    stats_err_locked();
               break;
          default:
               logger(LOG_NOTICE, "Invalid request <%d> from %s",
                      retval, inet_ntoa(data->client_info->client.sin_addr));
               tftp_send_error(data->sockfd, &data->client_info->client,
                               EBADOP, tftp_errmsg[EBADOP]);
               logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EBADOP,
                      tftp_errmsg[EBADOP]);
               stats_err_locked();
          }
     }  

     /* make sure all data is sent to the network */
     if (data->sockfd)
     {
          fsync(data->sockfd);
          close(data->sockfd);
     }

     /* update stats */
     stats_thread_usage_locked();

     /* Remove the thread_data structure from the list, if it as been
        added. */
     if (!abort)
          tftpd_list_remove(data);

     /* Free memory. */
     free(data->data_buffer);
     free(data->ack_buffer);
     free(data->tftp_options);

     /* Three things may append with client_info:
        1- We have only one client (probably not multicast)
        2- We do not have any client anymore, we transfered it to an other
        server
        3- We do not have any client, they are all done.
     */
     tftpd_clientlist_free(data->client_info);
     free(data);
     
     logger(LOG_INFO, "Server thread exiting");
     pthread_exit(NULL);
}

/*
 * If we receive signals, we must exit in a clean way. This means
 * sending an ERROR packet to all clients to terminate the connection.
 */
void signal_handler(int signal)
{
     switch (signal)
     {
     case SIGINT:
          /* oups, we received a sigint */
          /* 
           * if receiving or sending files, we should abort
           * and send an error ACK
           */
          /*
           * In other case, just exit
           */
          logger(LOG_ERR, "SIGINT caught. Aborting.");
          exit(ERR);
          break;
     default:
          logger(LOG_WARNING, "Should write an handler for signal %d",
                 signal);
          break;
     }
}

/*
 * Parse the command line using the standard getopt function.
 */
int tftpd_cmd_line_options(int argc, char **argv)
{
     int c;
//     int option_index = 0;
     static struct option options[] = {
          { "help", 0, NULL, 'h' },
          { "tftpd-timeout", 1, NULL, 't'},
          { "retry-timeout", 1, NULL, 'r'},
          { "verbose", 2, NULL, 'v'},
          { "maxthread", 1, NULL, 'm'},
          { "no-timeout", 0, NULL, 'T'},
          { "no-tsize", 0, NULL, 'S'},
          { "no-blksize", 0, NULL, 'B'},
          { "no-multicast", 0, NULL, 'M'},
          { "logfile", 1, NULL, 'L'},
          { "help", 0, NULL, 'h'},
          { "version", 0, NULL, 'V'},
          { 0, 0, 0, 0 }
     };
          
     while ((c = getopt_long(argc, argv, "Vht:r:v::m:",
                             options, NULL)) != EOF)
     {
          switch (c)
          {
          case 't':
               tftpd_timeout = atoi(optarg);
               break;
          case 'r':
               retry_timeout = atoi(optarg);
               break;
          case 'm':
               tftpd_max_thread = atoi(optarg);
               break;
          case 'v':
               if (optarg)
                    logging_level = atoi(optarg);
               else
                    logging_level++;
               break;
          case 'T':
               tftp_default_options[OPT_TIMEOUT].enabled = 0;
               break;
          case 'S':
               tftp_default_options[OPT_TSIZE].enabled = 0;
               break;
          case 'B':
               tftp_default_options[OPT_BLKSIZE].enabled = 0;
               break;
          case 'M':
               tftp_default_options[OPT_MULTICAST].enabled = 0;
               break;
          case 'L':
               log_file = strdup(optarg);
               break;
          case 'V':
               printf("%s\n", VERSION);
               exit(0);
          case 'h':
               tftpd_usage();
               exit(0);
          case '?':
               exit(1);
               break;
          }
     }

     /* verify that only one arguement is left */
     if (optind < argc)
          strncpy(directory, argv[optind], MAXLEN);
     /* make sure the last caracter is a / */
     if (directory[strlen(directory)] != '/')
          strcat(directory, "/");

     return OK;
}

/*
 * Output option to the syslog.
 */
void tftpd_print_options(void)
{
     logger(LOG_INFO, "  logging level: %d", logging_level);
     logger(LOG_INFO, "  directory: %s", directory);
     logger(LOG_INFO, "  log file: %s", (log_file==NULL) ? "syslog":log_file);
     logger(LOG_INFO, "  server timeout: %d", tftpd_timeout);
     logger(LOG_INFO, "  tftp retry timeout: %d", retry_timeout);
     logger(LOG_INFO, "  maximum number of thread: %d", tftpd_max_thread);
     logger(LOG_INFO, "  option timeout:   %s",
            tftp_default_options[OPT_TIMEOUT].enabled ? "enabled":"disabled");
     logger(LOG_INFO, "  option tzise:     %s",
            tftp_default_options[OPT_TSIZE].enabled ? "enabled":"disabled");
     logger(LOG_INFO, "  option blksize:   %s",
            tftp_default_options[OPT_BLKSIZE].enabled ? "enabled":"disabled");
     logger(LOG_INFO, "  option multicast: %s",
            tftp_default_options[OPT_MULTICAST].enabled ? "enabled":"disabled");
}

/*
 * Show a nice usage...
 */
void tftpd_usage(void)
{
     printf("*** tftpd must be called by inetd ***\n"
            "Usage: tftpd [options] [directory]\n"
            " [options] may be:\n"
            "  -t, --tftpd-timeout <value>: number of second of inactivity"
            " before exiting\n"
            "  -r, --retry-timeout <value>: time to wait a reply before"
            " retransmition\n"
            "  -m, --maxthread <value>    : number of concurrent thread"
            " allowed\n"
            "  -v, --verbose [value]      : increase or set the level of"
            " output messages\n"
            "  --no-timeout               : disable 'timeout' from RFC2349\n"
            "  --no-tisize                : disable 'tsize' from RFC2349\n"
            "  --no-blksize               : disable 'blksize' from RFC2348\n"
            "  --no-multicast             : disable 'multicast' from RFC2090\n"
            "  --logfile                  : logfile to log logs to ;-)\n"
            "  -h, --help                 : print this help\n"
            "  -V, --version              : print version information\n"
            "\n"
            " [directories] must be a world readable/writable\n"
            " directories. By default /tftpboot is assumed."
            "\n");
}
