/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftpd_file.c
 *    state machine for file transfer on the server side
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
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "tftpd.h"
#include "tftp_io.h"
#include "tftp_def.h"
#include "logger.h"
#include "options.h"

#define S_BEGIN         0
#define S_SEND_REQ      1
#define S_SEND_ACK      2
#define S_SEND_OACK     3
#define S_SEND_DATA     4
#define S_WAIT_PACKET   5
#define S_REQ_RECEIVED  6
#define S_ACK_RECEIVED  7
#define S_OACK_RECEIVED 8
#define S_DATA_RECEIVED 9
#define S_ABORT         10
#define S_END           11

/*
 * Receive a file. It is implemented as a state machine using a while loop
 * and a switch statement. Function flow is as follow:
 *  - sanity check
 *  - check client's request
 *  - enter state machine
 *
 *     1) send a ACK or OACK
 *     2) wait replay
 *          - if DATA packet, read it, send an acknoledge, goto 2
 *          - if ERROR abort
 *          - if TIMEOUT goto previous state
 */
int tftpd_receive_file(struct thread_data *data)
{
     int state = S_BEGIN;
     int timeout_state = state;
     int result;
     int block_number = 0;
     int data_size;
     int sockfd = data->sockfd;
     struct sockaddr_in *sa = &data->client_info->client;
     struct tftphdr *tftphdr = (struct tftphdr *)data->data_buffer;
     FILE *fp;
     char filename[MAXLEN];
     char string[MAXLEN];
     int timeout = data->timeout;
     int number_of_timeout = 0;
     int all_blocks_received = 0; /* temporary kludge */
     int convert = 0;           /* if true, do netascii convertion */

     /* look for mode option */
     if (strcasecmp(data->tftp_options[OPT_MODE].value, "netascii") == 0)
     {
          convert = 1;
          logger(LOG_DEBUG, "will do netascii convertion");
     }

     /* If the filename starts with the directory, allow it */
     if (strncmp(directory, data->tftp_options[OPT_FILENAME].value,
                 strlen(directory)) == 0)
          strcpy(filename, data->tftp_options[OPT_FILENAME].value);
     else
     {
          /* Append the prefix directory to the requested file. */
          strcpy(filename, directory);
          strcat(filename, data->tftp_options[OPT_FILENAME].value);
     }

     /* If the filename contain /../ sequences, we forbid the access */
     if (strstr(filename, "/../") != NULL)
     {
          /* Illegal access. */
          logger(LOG_INFO, "File name with /../ are forbidden");
          tftp_send_error(sockfd, sa, EACCESS, tftp_errmsg[EACCESS]);
          logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EACCESS,
                 tftp_errmsg[EACCESS]);
          return ERR;  
     }

     /* Open the file for writing. */
     if ((fp = fopen(filename, "w")) == NULL)
     {
          /* Can't create the file. */
          logger(LOG_INFO, "Can't open %s for writing", filename);
          tftp_send_error(sockfd, sa, EACCESS, tftp_errmsg[EACCESS]);
          logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EACCESS,
                 tftp_errmsg[EACCESS]);
          return ERR;
     }

     /* tsize option */
     if ((result = opt_get_tsize(data->tftp_options)) > -1)
     {
          opt_set_tsize(result, data->tftp_options);
          logger(LOG_DEBUG, "tsize option -> %d", result);
     }

     /* timeout option */
     if ((result = opt_get_timeout(data->tftp_options)) > -1)
     {
          if ((result < 1) || (result > 255))
          {
               tftp_send_error(sockfd, sa, EOPTNEG, tftp_errmsg[EOPTNEG]);
               logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EOPTNEG,
                      tftp_errmsg[EOPTNEG]);
               fclose(fp);
               return ERR;
          }
          timeout = result;
          opt_set_timeout(timeout, data->tftp_options);
          logger(LOG_DEBUG, "timeout option -> %d", timeout);
     }

     /* blksize options */
     if ((result = opt_get_blksize(data->tftp_options)) > -1)
     {
          if ((result < 8) || (result > 65464))
          {
               tftp_send_error(sockfd, sa, EOPTNEG, tftp_errmsg[EOPTNEG]);
               logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EOPTNEG,
                      tftp_errmsg[EOPTNEG]);
               fclose(fp);
               return ERR;
          }
          data->data_buffer_size = result + 4;
          data->data_buffer = realloc(data->data_buffer,
                                      data->data_buffer_size);
          tftphdr = (struct tftphdr *)data->data_buffer;

          if (data->data_buffer == NULL)
          {
               tftp_send_error(sockfd, sa, ENOSPACE, tftp_errmsg[ENOSPACE]);
               logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", ENOSPACE,
                      tftp_errmsg[ENOSPACE]);
               fclose(fp);
               return ERR;
          }
          opt_set_blksize(result, data->tftp_options);
          logger(LOG_DEBUG, "blksize option -> %d", result);
     }

     /* that's it, we start receiving the file */
     while (1)
     {
          switch (state)
          {
          case S_BEGIN:
               /* Did the client request RFC1350 options ?*/
               if (opt_support_options(data->tftp_options))
                    state = S_SEND_OACK;
               else
                    state = S_SEND_ACK;
               break;
          case S_SEND_ACK:
               timeout_state = state;
               tftp_send_ack(sockfd, sa, block_number);
               logger(LOG_DEBUG, "sent ACK <block: %d>", block_number);
               if (all_blocks_received)
                    state = S_END;
               else
                    state = S_WAIT_PACKET;
               break;
          case S_SEND_OACK:
               timeout_state = state;
               tftp_send_oack(sockfd, sa, data->tftp_options);
               opt_options_to_string(data->tftp_options, string, MAXLEN);
               logger(LOG_DEBUG, "sent OACK <%s>", string);
               state = S_WAIT_PACKET;
               break;
          case S_WAIT_PACKET:
               data_size = data->data_buffer_size;
               result = tftp_get_packet(sockfd, sa, timeout,
                                        &data_size, data->data_buffer);
               switch (result)
               {
               case GET_TIMEOUT:
                    number_of_timeout++;
                    if (number_of_timeout > NB_OF_RETRY)
                         state = S_ABORT;
                    else
                    {
                         if (timeout_state != S_END)
                              logger(LOG_WARNING, "timeout: retrying...");
                         else
                              logger(LOG_WARNING, "timeout: exiting");
                         state = timeout_state;
                    }
                    break;
               case GET_ERROR:
                    strncpy(string, tftphdr->th_msg,
                            (((data_size - 4) > MAXLEN) ? MAXLEN :
                             (data_size - 4)));
                    logger(LOG_DEBUG, "received ERROR <code: %d, msg: %s>",
                           ntohs(tftphdr->th_code), string);
                    state = S_ABORT;
                    break;
               case GET_DATA:
                    state = S_DATA_RECEIVED;
                    break;
               case GET_DISCARD:
                    logger(LOG_WARNING, "packet discarded");
                    break;
               case ERR:
                    logger(LOG_ERR, "%s: %d: recvfrom: %s",
                           __FILE__, __LINE__, strerror(errno));
                    state = S_ABORT;
                    break;
               default:
                    logger(LOG_ERR, "%s: %d: abnormal return value %d",
                           __FILE__, __LINE__, result);
                    state = S_ABORT;
               }
               break;
          case S_DATA_RECEIVED:
               /* We need to seek to the right place in the file */
               block_number = ntohs(tftphdr->th_block);
               logger(LOG_DEBUG, "received DATA <block: %d, size: %d>",
                      block_number, data_size - 4);
               fseek(fp, (block_number - 1) * (data->data_buffer_size - 4),
                     SEEK_SET);
               if (fwrite(tftphdr->th_data, 1, data_size - 4, fp) !=
                   (data_size - 4))
               {
                    logger(LOG_ERR, "%s: %d: error writing to file %s",
                           __FILE__, __LINE__, filename);
                    tftp_send_error(sockfd, sa, ENOSPACE,
                                    tftp_errmsg[ENOSPACE]);
                    logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>",
                           ENOSPACE, tftp_errmsg[ENOSPACE]);
                    state = S_ABORT;
                    break;
               }
               if (data_size < data->data_buffer_size)
                    all_blocks_received = 1;
               else
                    all_blocks_received = 0;
               state = S_SEND_ACK;
               break;
          case S_END:
               fclose(fp);
               return OK;
          case S_ABORT:
               fclose(fp);
               return ERR;
          default:
               fclose(fp);
               logger(LOG_ERR, "%s: %d: tftpd_file.c: huh?",
                      __FILE__, __LINE__);
               return ERR;
          }
     }
}

/*
 * Send a file. It is implemented as a state machine using a while loop
 * and a switch statement. Function flow is as follow:
 *  - sanity check
 *  - check client's request
 *  - enter state machine
 *
 *     1) send a DATA or OACK
 *     2) wait replay
 *          - if ACK, goto 3
 *          - if ERROR abort
 *          - if TIMEOUT goto previous state
 *     3) send data, goto 2
 */
int tftpd_send_file(struct thread_data *data)
{
     int state = S_BEGIN;
     int timeout_state = state;
     int result;
     int block_number = 0;
     int data_size;
     struct sockaddr_in *sa = &data->client_info->client;
     int sockfd = data->sockfd;
     struct tftphdr *tftphdr = (struct tftphdr *)data->data_buffer;
     FILE *fp;
     char filename[MAXLEN];
     char string[MAXLEN];
     int timeout = data->timeout;
     int number_of_timeout = 0;
     struct stat file_stat;
     int convert = 0;           /* if true, do netascii convertion */
     struct thread_data *thread = NULL; /* used when looking for a multicast
                                           thread */

     /* look for mode option */
     if (strcasecmp(data->tftp_options[OPT_MODE].value, "netascii") == 0)
     {
          convert = 1;
          logger(LOG_DEBUG, "will do netascii convertion");
     }

     /* fetch the file name */
     /* If the filename starts with the directory, allow it */
     if (strncmp(directory, data->tftp_options[OPT_FILENAME].value,
                 strlen(directory)) == 0)
          strcpy(filename, data->tftp_options[OPT_FILENAME].value);
     else
     {
          strcpy(filename, directory);
          strcat(filename, data->tftp_options[OPT_FILENAME].value);
     }

     /* If the filename contain /../ sequences, we forbid the access */
     if (strstr(filename, "/../") != NULL)
     {
          /* Illegal access. */
          logger(LOG_INFO, "File name with /../ are forbidden");
          tftp_send_error(sockfd, sa, EACCESS, tftp_errmsg[EACCESS]);
          logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EACCESS,
                 tftp_errmsg[EACCESS]);
          return ERR;  
     }

     /* verify that the requested file exist */
     if ((fp = fopen(filename, "r")) == NULL)
     {
          
          tftp_send_error(sockfd, sa, ENOTFOUND, tftp_errmsg[ENOTFOUND]);
          logger(LOG_INFO, "File %s not found", filename);
          logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", ENOTFOUND,
                 tftp_errmsg[ENOTFOUND]);
          return ERR;
     }   

     /* To return the size of the file with tsize argument */
     fstat(fileno(fp), &file_stat);
     /* tsize option */
     if (opt_get_tsize(data->tftp_options) > -1)
     {
          opt_set_tsize(file_stat.st_size, data->tftp_options);
          logger(LOG_INFO, "tsize option -> %d", file_stat.st_size);
     }

     /* timeout option */
     if ((result = opt_get_timeout(data->tftp_options)) > -1)
     {
          if ((result < 1) || (result > 255))
          {
               tftp_send_error(sockfd, sa, EOPTNEG, tftp_errmsg[EOPTNEG]);
               logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EOPTNEG,
                      tftp_errmsg[EOPTNEG]);
               fclose(fp);
               return ERR;
          }
          timeout = result;
          opt_set_timeout(timeout, data->tftp_options);
          logger(LOG_INFO, "timeout option -> %d", timeout);
     }

     /* blksize options */
     if ((result = opt_get_blksize(data->tftp_options)) > -1)
     {
          if ((result < 8) || (result > 65464))
          {
               tftp_send_error(sockfd, sa, EOPTNEG, tftp_errmsg[EOPTNEG]);
               logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EOPTNEG,
                      tftp_errmsg[EOPTNEG]);
               fclose(fp);
               return ERR;
          }
          data->data_buffer_size = result + 4;
          data->data_buffer = realloc(data->data_buffer,
                                      data->data_buffer_size);
          tftphdr = (struct tftphdr *)data->data_buffer;

          if (data->data_buffer == NULL)
          {
               tftp_send_error(sockfd, sa, ENOSPACE, tftp_errmsg[ENOSPACE]);
               logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", ENOSPACE,
                      tftp_errmsg[ENOSPACE]);
               fclose(fp);
               return ERR;
          }
          opt_set_blksize(result, data->tftp_options);
          logger(LOG_INFO, "blksize option -> %d", result);
     }


     /* multicast option */
     if (data->tftp_options[OPT_MULTICAST].specified &&
         data->tftp_options[OPT_MULTICAST].enabled)
     {
          //pthread_mutex_lock(&thread_list_mutex);
          /* find a server with the same options */
          if (tftpd_list_find_multicast_server(&thread, data->tftp_options))
          {
               //pthread_mutex_unlock(&thread_list_mutex);
               /* add this client to its list of client */
               //pthread_mutex_lock(&thread->mutex);

               //pthread_mutex_unlock(&thread->mutex);

               return ERR;
          }
          //pthread_mutex_unlock(&thread_list_mutex);
          /* configure socket, get an IP address */

          /* set options data for OACK */
          opt_set_options(data->tftp_options, "multicast", "");
     }

     /* That's it, ready to send the file */
     while (1)
     {
          switch (state)
          {
          case S_BEGIN:
               /* Did the client request RFC1350 options ?*/
               if (opt_support_options(data->tftp_options))
                    state = S_SEND_OACK;
               else
                    state = S_SEND_DATA;
               break;
          case S_SEND_OACK:
               timeout_state = state;          
               tftp_send_oack(sockfd, sa, data->tftp_options);
               opt_options_to_string(data->tftp_options, string, MAXLEN);
               logger(LOG_DEBUG, "sent OACK <%s>", string);            
               state = S_WAIT_PACKET;
               break;
          case S_SEND_DATA:
               timeout_state = state;
               if (feof(fp))
               {
                    state = S_END;
                    break;
               }
               fseek(fp, block_number * (data->data_buffer_size - 4),
                     SEEK_SET);
               data_size = fread(tftphdr->th_data, 1,
                                 data->data_buffer_size - 4, fp) + 4;
               tftp_send_data(sockfd, sa, block_number + 1,
                              data_size, data->data_buffer);
               logger(LOG_DEBUG, "sent DATA <block: %d, size %d>",
                      block_number + 1, data_size - 4);
               state = S_WAIT_PACKET;
               break;
          case S_WAIT_PACKET:
               data_size = data->data_buffer_size;
               result = tftp_get_packet(sockfd, sa, timeout,
                                        &data_size, data->data_buffer);
               switch (result)
               {
               case GET_TIMEOUT:
                    number_of_timeout++;
                    if (number_of_timeout > NB_OF_RETRY)
                         state = S_ABORT;
                    else
                    {
                         logger(LOG_WARNING, "timeout: retrying...");
                         state = timeout_state;
                    }
                    break;
               case GET_ACK:
                    block_number = ntohs(tftphdr->th_block);
                    logger(LOG_DEBUG, "received ACK <block: %d>",
                           block_number);
                    state = S_SEND_DATA;
                    break;
               case GET_ERROR:
                    strncpy(string, tftphdr->th_msg,
                            (((data_size - 4) > MAXLEN) ? MAXLEN :
                             (data_size - 4)));
                    logger(LOG_DEBUG, "received ERROR <code: %d, msg: %s>",
                           ntohs(tftphdr->th_code), string);
                    state = S_ABORT;
                    break;
               case GET_DISCARD:
                    logger(LOG_WARNING, "packet discarded");
                    break;
               case ERR:
                    logger(LOG_ERR, "%s: %d: recvfrom: %s",
                           __FILE__, __LINE__, strerror(errno));
                    state = S_ABORT;
                    break;
               default:
                    logger(LOG_ERR, "%s: %d: abnormal return value %d",
                           __FILE__, __LINE__, result);
               }
               break;
          case S_END:
               fclose(fp);
               return OK;
          case S_ABORT:
               fclose(fp);
               return ERR;
          default:
               fclose(fp);
               logger(LOG_ERR, "%s: %d: huh?", __FILE__, __LINE__);
               return ERR;
          }
     }
}
