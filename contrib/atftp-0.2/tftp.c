/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftp.c
 *    main client file.
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
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h> 

#include <readline/readline.h>
#include <readline/history.h>
#include <argz.h>

#include "tftp.h"
#include "tftp_io.h"
#include "tftp_def.h"
#include "logger.h"
#include "options.h"

/* Maximum number of args on a line. 20 should be more than enough. */
#define MAXARG  20

/*
 * General options. Used in tftp.c and tftp_file.c only.
 */
int trace = 0;                  /* debugging information */
int verbose = 0;                /* to print message at each step */

/* local flags */
int interactive = 1;            /* if false, we run in batch mode */


/* Structure to hold some information that must be passed to
 * functions.
 */
struct client_data data;

/* tftp.c local only functions. */
void signal_handler(int signal);
int read_cmd(void);
void make_arg(char *string, int *argc, char ***argv);
int process_cmd(int argc, char **argv);
int tftp_cmd_line_options(int argc, char **argv);
void tftp_usage(void);

/* Functions associated with the tftp commands. */
int set_peer(int argc, char **argv);
int set_mode(int argc, char **argv);
int set_option(int argc, char **argv);
int put_file(int argc, char **argv);
int get_file(int argc, char **argv);
int quit(int argc, char **argv);
int set_verbose(int argc, char **argv);
int set_trace(int argc, char **argv);
int status(int argc, char **argv);
int set_timeout(int argc, char **argv);
int help(int argc, char **argv);

/* All supported commands. */
struct command {
     const char *name;
     int (*func)(int argc, char **argv);
     const char *helpmsg;
} cmdtab[] = {
     {"connect", set_peer, "connect to tftp server"},
     {"mode", set_mode, "set file transfer mode (netascii/octet)"},
     {"option", set_option, "set RFC1350 options"},
     {"put", put_file, "upload a file to the host"},
     {"get", get_file, "download a file from the host"},
     {"quit", quit, "exit tftp"},
     {"verbose", set_verbose, "toggle verbose mode"},
     {"trace", set_trace, "toggle trace mode"},
     {"status", status, "print status information"},
     {"timeout", set_timeout, "set the timeout before a retry"},
     {"help", help, "print help message"},
     {"?", help, "print help message"},
     {NULL, NULL, NULL}
};

/* Defined in tftp_file.c and only called in this file. */
int tftp_receive_file(struct client_data *data);
int tftp_send_file(struct client_data *data);

/*
 * Open the socket, register signal handler and
 * pass the control to read_cmd.
 */
int main(int argc, char **argv)
{
     socklen_t len = sizeof(data.sa_local);

     /* Allocate memory for data buffer. */
     if ((data.data_buffer = malloc((size_t)SEGSIZE+4)) == NULL)
     {
          fprintf(stderr, "tftp: memory allcoation failed.\n");
          exit(ERR);
     }
     data.data_buffer_size = SEGSIZE + 4;

     /* Allocate memory for tftp option structure. */
     if ((data.tftp_options = 
          malloc(sizeof(tftp_default_options))) == NULL)
     {
          fprintf(stderr, "tftp: memory allocation failed.\n");
          exit(ERR);
     }
     /* Copy default options. */
     memcpy(data.tftp_options, tftp_default_options,
            sizeof(tftp_default_options));   

     /* Allocate memory for tftp option reply from server. */
     if ((data.tftp_options_reply = 
          malloc(sizeof(tftp_default_options))) == NULL)
     {
          fprintf(stderr, "tftp: memory allocation failed.\n");
          exit(ERR);
     }
     /* Copy default options. */
     memcpy(data.tftp_options_reply, tftp_default_options,
            sizeof(tftp_default_options));

     /* get the server entry */
     data.sp = getservbyname("tftp", "udp");
     if (data.sp == 0) {
          fprintf(stderr, "tftp: udp/tftp, unknown service.\n");
          exit(ERR);
     }

     /* open a UDP socket */
     data.sockfd = socket(PF_INET, SOCK_DGRAM, 0);
     memset(&data.sa_local, 0, sizeof(data.sa_local));
     bind(data.sockfd, &data.sa_local, sizeof(data.sa_local));
     getsockname(data.sockfd, (struct sockaddr *)&data.sa_local, &len);
     if (data.sockfd < 0) {
          perror("tftp: ");
          exit(ERR);
     }

     /* default timeout value */
     data.timeout = TIMEOUT;

     /* register SIGINT -> C-c */
     signal(SIGINT, signal_handler);

     /* parse options, and maybe run in non interractive mode */
     tftp_cmd_line_options(argc, argv);

     if (interactive)
          return read_cmd();
     return OK;
}

void signal_handler(int signal)
{
     /* oups, we receive a sigint */
     /* 
      * if receiving or sending files, we should abort
      * and send and error ACK
      */
     /*
      * In other case, just exit
      */
     fprintf(stderr, "Ouch! SIGINT received\n");
     exit(ERR);
}

/*
 * Read command with a nice prompt and history :)
 */
int read_cmd(void)
{
     char *string = NULL;

     int argc;
     char **argv = NULL;

     using_history();

     while (1)
     {
          if ((string = readline("tftp> ")) == NULL)
               break;
          if (strlen(string) != 0)
          {
               make_arg(string, &argc, &argv);
               if (argc > 0)
               {
                    add_history(string);
                    process_cmd(argc, argv);
               }
               free(string);
          }
     }
     return 0;
}

/*
 * Split a string into args.
 */
void make_arg(char *string, int *argc, char ***argv)
{
     static char *tmp = NULL;
     int argz_len;

     /* split the string to an argz vector */
     if (argz_create_sep(string, ' ', &tmp, &argz_len) != 0)
     {
          *argc = 0;
          return;
     }
     /* retreive the number of arguments */
     *argc = argz_count(tmp, argz_len);
     /* give enough space for all arguments to **argv */
     if ((*argv = realloc(*argv,  *argc + 1)) == NULL)
     {
          *argc = 0;
          return;
     }
     /* extract arguments */
     argz_extract (tmp, argz_len, *argv);
     
     /* if the last argument is an empty string ... it appen
        when some extra spece are added at the end of string :( */
     if (strlen((*argv)[*argc - 1]) == 0)
          *argc = *argc - 1;
}

/*
 * Once a line have been read and splitted, find the corresponding
 * function and call it.
 */
int process_cmd(int argc, char **argv)
{
     int i = 0;

     /* find the command in the command table */
     while (1)
     {
          if (cmdtab[i].name == NULL)
          {
               fprintf(stderr, "tftp: bad command name.\n");
               return 0;
          }
          if (strcasecmp(cmdtab[i].name, argv[0]) == 0)
          {
               return (cmdtab[i].func)(argc, argv);
          }
          i++;
     }
}

/*
 * Update sa_peer structure, hostname and port number adequately
 */
int set_peer(int argc, char **argv)
{
     struct hostent *host;

     /* sanity check */
     if ((argc < 2) || (argc > 3))
     {
          fprintf(stderr, "Usage: %s host-name [port]\n", argv[0]);
          return ERR;
     }
     /* look up the host */
     host = gethostbyname(argv[1]);
     /* if valid, update s_inn structure */
     if (host)
     {
          data.sa_peer.sin_family = host->h_addrtype;
          if (host->h_length > sizeof(data.sa_peer.sin_addr))
               host->h_length = sizeof(data.sa_peer.sin_addr);
          memcpy(&data.sa_peer.sin_addr, host->h_addr, host->h_length);
          strncpy(data.hostname, host->h_name,
                  sizeof(data.hostname));
          data.hostname[sizeof(data.hostname)-1] = 0;
          data.sa_peer.sin_port = data.sp->s_port;
     } 
     else
     {
          fprintf(stderr, "tftp: unknown host %s.\n", argv[1]);
          data.connected = 0;
          return ERR;
     }
     /* get the server port */
     if (argc == 3)
     {
          data.sp->s_port = htons(atoi(argv[2]));
          if (data.sp->s_port < 0)
          {
               fprintf(stderr, "%s: bad port number.\n", argv[2]);
               data.connected = 0;
               return ERR;
          }
          data.sa_peer.sin_port = data.sp->s_port;
     }
     data.connected = 1;
     return OK;
}

/*
 * Set the mode to netascii or octet
 */
int set_mode(int argc, char **argv)
{
     if (argc > 2)
     {
          fprintf(stderr, "Usage: %s [netascii] [octet]\n", argv[0]);
          return ERR;
     }
     if (argc == 1)
     {
          fprintf(stderr, "Current mode is %s.\n",
                  data.tftp_options[OPT_MODE].value);
          return OK;
     }
     if (strcasecmp("netascii", argv[1]) == 0)
          strncpy(data.tftp_options[OPT_MODE].value, "netascii",
                  VAL_SIZE);
     else if (strcasecmp("octet", argv[1]) == 0)
          strncpy(data.tftp_options[OPT_MODE].value, "octet",
                  VAL_SIZE);
     else
     {
          fprintf(stderr, "tftp: unkowned mode %s.\n", argv[1]);
          fprintf(stderr, "Usage: %s [netascii] [octet]\n", argv[0]);
          return ERR;
     }
     return OK;
}

/*
 * Set an option
 */
int set_option(int argc, char **argv)
{
     /* if there are no arguments */
     if ((argc < 2) || (argc > 3))
     {
          fprintf(stderr, "Usage: option <option name> [option value]\n");
          fprintf(stderr, "       option disable <option name>\n");
          return ERR;
     }
     /* if disabling an option */
     if (strcasecmp("disable", argv[1]) == 0)
     {
          if (argc != 3)
          {
               fprintf(stderr, "Usage: option disable <option name>\n");
               return ERR;
          }
          /* disable it */
          opt_disable_options(data.tftp_options, argv[2]);
          return OK;
     }
     /* ok, we are setting an argument */
     if (argc == 2)
          opt_set_options(data.tftp_options, argv[1], NULL);
     if (argc == 3)
          opt_set_options(data.tftp_options, argv[1], argv[2]);
     return OK;
}

/*
 * Put a file to the server.
 */
int put_file(int argc, char **argv)
{
     if ((argc < 2) || (argc > 3))
     {
          fprintf(stderr, "Usage: put local_file [remote_file]\n");
          return ERR;
     }
     if (!data.connected)
     {
          fprintf(stderr, "Not connected.\n");
          return ERR;
     }
     if (argc == 2)
     {
          strncpy(data.local_file, argv[1], VAL_SIZE);
          strncpy(data.tftp_options[OPT_FILENAME].value, argv[1], VAL_SIZE);
     }
     else
     {
          strncpy(data.local_file, argv[1], VAL_SIZE);
          strncpy(data.tftp_options[OPT_FILENAME].value, argv[2], VAL_SIZE);
     }
     tftp_send_file(&data);
     return OK;
}

/*
 * Receive a file from the server.
 */
int get_file(int argc, char **argv)
{
     char *string;
     FILE *fp;

     if ((argc < 2) || (argc > 3))
     {
          fprintf(stderr, "Usage: get remote_file [local_file]\n");
          return ERR;
     }
     if (!data.connected)
     {
          fprintf(stderr, "Not connected.\n");
          return ERR;
     }
     if (argc == 2)
     {
          strncpy(data.tftp_options[OPT_FILENAME].value,
                  argv[1], VAL_SIZE);
          string = strrchr(argv[1], '/');
          if (string < argv[1])
               string = argv[1];
          else
               string++;
          strncpy(data.local_file, string, VAL_SIZE);
     }
     else
     {
          strncpy(data.local_file, argv[2], VAL_SIZE);
          strncpy(data.tftp_options[OPT_FILENAME].value, argv[1], VAL_SIZE);
     }

     /* if interractive, verify if localfile exists */
     if (interactive)
     {
          /* if localfile if stdout, nothing to verify */
          if (strcmp(data.local_file, "/dev/stdout") != 0)
          {
               if ((fp = fopen(data.local_file, "r")) != NULL)
               {
                    fclose(fp);
                    string = readline("Overwite local file [y/n]? ");
                    if (!(strcasecmp(string, "y") == 0))
                    {
                         free(string);
                         return OK;
                    }
                    free(string);
               }
          }
     }
     tftp_receive_file(&data);
     return OK;
}

/*
 * Exit tftp
 */
int quit(int argc, char **argv)
{
     exit(OK);
}

int set_verbose(int argc, char **argv)
{
     if (verbose)
     {
          verbose = 0;
          fprintf(stderr, "Verbose mode off.\n");
     }
     else
     {
          verbose = 1;
          fprintf(stderr, "Verbose mode on.\n");
     }
     return OK;
}

int set_trace(int argc, char **argv)
{
     if (trace)
     {
          trace = 0;
          fprintf(stderr, "Trace mode off.\n");
     }
     else
     {
          trace = 1;
          fprintf(stderr, "Trace mode on.\n");
     }
     return OK;
}

int status(int argc, char **argv)
{
     if (!data.connected)
          fprintf(stderr, "Not connected\n");
     else
          fprintf(stderr, "Connected to %s\n", data.hostname);
     fprintf(stderr, "Mode: %s\n", data.tftp_options[OPT_MODE].value);
     if (verbose)
          fprintf(stderr, "Verbose: on\n");
     else
          fprintf(stderr, "Verbose: off\n");
     if (trace)
          fprintf(stderr, "Trace: on\n");
     else
          fprintf(stderr, "Trace: off\n");
     fprintf(stderr, "Options:\n");
     if (data.tftp_options[OPT_TSIZE].specified)
          fprintf(stderr, "\ttsize enabled\n");
     else
          fprintf(stderr, "\ttsize disabled\n");
     if (data.tftp_options[OPT_BLKSIZE].specified)
          fprintf(stderr, "\tblksize enabled\n");
     else
          fprintf(stderr, "\tblksize disabled\n");
     if (data.tftp_options[OPT_TIMEOUT].specified)
          fprintf(stderr, "\ttimeout enabled\n");
     else
          fprintf(stderr, "\ttimeout disabled\n");
     if (data.tftp_options[OPT_MULTICAST].specified)
          fprintf(stderr, "\tmulticast enabled\n");
     else
          fprintf(stderr, "\tmulticast disabled\n");
     return OK;
}

int set_timeout(int argc, char **argv)
{
     if (argc == 1)
          fprintf(stderr, "Timeout set to %d seconds.\n", data.timeout);
     if (argc == 2)
          data.timeout = atoi(argv[1]);
     if (argc > 2)
     {
          fprintf(stderr, "Usage: timeout [value]\n");
          return ERR;
     }
     return OK;
}

int help(int argc, char **argv)
{
     int i = 0;

     if (argc == 1)
     {
          /* general help */
          fprintf(stderr, "Available command are:\n");
          while (cmdtab[i].name != NULL)
          {
               fprintf(stderr, "%s\t\t%s\n", cmdtab[i].name,
                       cmdtab[i].helpmsg);
               i++;
          }
     }
     if (argc > 1)
     {
          while (cmdtab[i].name != NULL)
          {
               if (strcasecmp(cmdtab[i].name, argv[1]) == 0)
                    fprintf(stderr, "%s: %s\n", cmdtab[i].name,
                            cmdtab[i].helpmsg);
               i++;
          }
     }
     return OK;
}

#define PUT 1
#define GET 2

/*
 * If tftp is called with --help, usage is printed and we exit.
 * With --version, version is printed and we exit too.
 * if --get --put --remote-file or --local-file is set, it imply non
 * interactive invocation of tftp.
 */
int tftp_cmd_line_options(int argc, char **argv)
{
     int c;
     int ac;                    /* to format arguments for process_cmd */
     char **av = NULL;          /* same */
     char string[MAXLEN];
     char local_file[MAXLEN] = " ";
     char remote_file[MAXLEN] = " ";
     int action = 0;

     int option_index = 0;
     static struct option options[] = {
          { "help", 0, NULL, 'h' },
          { "trace", 0, NULL, 'v'},
          { "tftp-timeout", 1, NULL, 'T'},
          { "timeout", 1, NULL, 't'},
          { "blksize", 1, NULL, 'b'},
          { "tsize", 0, NULL, 's'},
          { "multicast", 0, NULL, 'm'},
          { "local-file", 1, NULL, 'l'},
          { "remote-file", 1, NULL, 'r'},
          { "get", 0, NULL, 'g'},
          { "put", 0, NULL, 'p'},
          { "version", 0, NULL, 'V'},
          { 0, 0, 0, 0 }
     };

     while ((c = getopt_long(argc, argv, "hvT:t:b:sml:r:gpV",
                             options, &option_index)) != EOF)
     {
          switch (c)
          {
          case 'v':
               sprintf(string, "trace on");
               make_arg(string, &ac, &av);
               process_cmd(ac, av);            
               break;
          case 'T':
               sprintf(string, "timeout %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 't':
               sprintf(string, "option timeout %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 'b':
               sprintf(string, "option blksize %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 's':
               sprintf(string, "option tsize");
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 'm':
               sprintf(string, "option multicast");
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 'l':
               interactive = 0;
               strncpy(local_file, optarg, MAXLEN);
               break;
          case 'r':
               interactive = 0;
               strncpy(remote_file, optarg, MAXLEN);
               break;
          case 'g':
               interactive = 0;
               if (action == PUT)
               {
                    fprintf(stderr, "two actions specified!\n");
                    exit(ERR);
               }
               else
                    action = GET;
               break;
          case 'p':
               interactive = 0;
               if (action == GET)
               {
                    fprintf(stderr, "two actions specified!\n");
                    exit(ERR);
               }
               else
                    action = PUT;
               break;
          case 'V':
               printf("%s\n", VERSION);
               exit(OK);
          case 'h':
               tftp_usage();
               exit(OK);
          case '?':
               tftp_usage();
               exit(ERR);
               break;
          }
     }

     /* verify that one or two arguements are left, they are the host name
        and port */
     /* optind point to the first non option caracter */
     if (optind < (argc - 2))
     {
          tftp_usage();
          exit(ERR);
     }

     if (optind != argc)
     {
          if (optind == (argc - 1))
               sprintf(string, "connect %s", argv[optind]);
          else
               sprintf(string, "connect %s %s", argv[optind], argv[optind+1]);
          make_arg(string, &ac, &av);
          process_cmd(ac, av);
     }
     
     if (!interactive)
     {
          if (action == PUT)
               sprintf(string, "put %s %s", local_file, remote_file);
          else if (action == GET)
               sprintf(string, "get %s %s", remote_file, local_file);
          else
          {
               fprintf(stderr, "No action specified in batch mode!\n");
               exit(ERR);
          }
          make_arg(string, &ac, &av);
          process_cmd(ac, av);
     }
     return OK;
}

void tftp_usage(void)
{
     fprintf(stderr,
             "Usage: tftp [options] [host] [port]\n"
             " [options] may be:\n"
             "  -v, --verbose              : set trace on\n"
             "  -T, --tftp-timeout <value> : delay before"
             " retransmission, client side)\n"
             "  -t, --timeout <value>      : delay before"
             " retransmition, server side (RFC2349)\n"
             "  -b, --blocksize <value>    : specify blksize (RFC2348)\n"
             "  -t, --tsize                : use 'tsize' from RFC2349\n"
             "  -m, --multicast            : use 'multicast' from RFC2090\n"
             "  -l, --local-file <file>    : local file to use\n"
             "  -r, --remote-file <file>   : remote file name\n"
             "  -g, --get                  : get file\n"
             "  -p, --put                  : put file\n"
             "  -h, --help                 : print this help\n"
             "  -V, --version              : print version information\n"
             "\n"
             " [host] is the tftp server name\n"
             " [port] is the port to use\n" 
             "\n");     
}
