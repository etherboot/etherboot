/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftp_file.c
 *    client side file operations. File receiving and sending.
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
#include <arpa/tftp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include "tftp.h"
#include "tftp_io.h"
#include "tftp_def.h"
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
 * Receive a file. This is implemented as a state machine using a while loop
 * and a switch statement. Function flow is as follow:
 *  - sanity check
 *  - enter state machine
 *
 *     1) send request
 *     2) wait replay
 *          - if DATA packet, read it, send an acknoledge, goto 2
 *          - if OACK (option acknowledge) acknowledge this option, goto 2
 *          - if ERROR abort
 *          - if TIMEOUT goto previous state
 */
int tftp_receive_file(struct client_data *data)
{
     int state = S_SEND_REQ;    /* current state in the state machine */
     int timeout_state;         /* what state should we go on when timeout */
     int result;
     int block_number = 0;
     int data_size;             /* size of data received */
     int sockfd = data->sockfd; /* just to simplify calls */
     struct sockaddr_in sa;     /* a copy of data.sa_peer */
     int connected;             /* 1 when sockfd is connected */
     struct tftphdr *tftphdr = (struct tftphdr *)data->data_buffer;
     FILE *fp = NULL;           /* the local file pointer */
     int number_of_timeout = 0;
     int all_blocks_received = 0; /* temporary kludge */
     int convert = 0;           /* if true, do netascii convertion */

     /* make sure the socket is not connected */
     sa.sin_family = AF_UNSPEC;
     connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
     connected = 0;

     /* copy sa_peer structure */
     memcpy(&sa, &data->sa_peer, sizeof(sa));

     /* check to see if conversion is requiered */
     if (strcasecmp(data->tftp_options[OPT_MODE].value, "netascii") == 0)
          convert = 1;

     /* make sure the data buffer is SEGSIZE + 4 bytes */
     if (data->data_buffer_size != (SEGSIZE + 4))
     {
          data->data_buffer = realloc(data->data_buffer, SEGSIZE + 4);
          tftphdr = (struct tftphdr *)data->data_buffer;
          if (data->data_buffer == NULL)
          {
               fprintf(stderr, "tftp: memory allocation failure.\n");
               exit(1);
          }
          data->data_buffer_size = SEGSIZE + 4;
     }

     /* open the file for writing */
     if ((fp = fopen(data->local_file, "w")) == NULL)
     {
          fprintf(stderr, "tftp: can't open %s for writing.\n",
                  data->local_file);
          return ERR;
     }

     while (1)
     {
          switch (state)
          {
          case S_SEND_REQ:
               timeout_state = S_SEND_REQ;
               if (trace)
                    fprintf(stderr, "sent RRQ <file: %s, mode: %s>\n",
                            data->tftp_options[OPT_FILENAME].value,
                            data->tftp_options[OPT_MODE].value);
               /* send request packet */
               if (tftp_send_request(sockfd, &sa, RRQ, data->data_buffer,
                                     data->data_buffer_size,
                                     data->tftp_options) == ERR)
                    state = S_ABORT;
               else
                    state = S_WAIT_PACKET;
               break;
          case S_SEND_ACK:
               timeout_state = S_SEND_ACK;
               if (trace)
                    fprintf(stderr, "sent ACK <block: %d>\n", block_number);
               tftp_send_ack(sockfd, &sa, block_number);
               if (all_blocks_received)
                    state = S_END;
               else
                    state = S_WAIT_PACKET;
               break;
          case S_WAIT_PACKET:
               data_size = data->data_buffer_size;
               result = tftp_get_packet(sockfd, &sa, data->timeout,
                                        &data_size, data->data_buffer);

               switch (result)
               {
               case GET_TIMEOUT:
                    number_of_timeout++;
                    fprintf(stderr, "timeout: retrying...\n");
                    if (number_of_timeout > NB_OF_RETRY)
                         state = S_ABORT;
                    else
                         state = timeout_state;
                    break;
               case GET_OACK:
                    /* if the socket if not connected, connect it */
                    if (!connected)
                    {
                         connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
                         connected = 1;
                    }
                    state = S_OACK_RECEIVED;
                    break;
               case GET_ERROR:
                    fprintf(stderr, "tftp: error received from server <");
                    fwrite(tftphdr->th_msg, 1, data_size - 4, stderr);
                    fprintf(stderr, ">\n");
                    state = S_ABORT;
                    break;
               case GET_DATA:
                    /* if the socket if not connected, connect it */
                    if (!connected)
                    {
                         connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
                         connected = 1;
                    }
                    state = S_DATA_RECEIVED;
                    break;
               case GET_DISCARD:
                    fprintf(stderr, "tftp: packet discard.\n");
                    break;
               case ERR:
                    fprintf(stderr, "tftp: unknown error.\n");
                    state = S_ABORT;
                    break;
               default:
                    fprintf(stderr, "tftp: abnormal return value %d.\n",
                            result);
               }
               break;
          case S_OACK_RECEIVED:
               /* clean the tftp_options structure */
               memcpy(data->tftp_options_reply, tftp_default_options,
                      sizeof(tftp_default_options));
               /*
                * look in the returned string for tsize, timeout, blksize or
                * multicast
                */
               opt_disable_options(data->tftp_options_reply, NULL);
               opt_parse_options(data->data_buffer, data_size,
                                 data->tftp_options_reply);
               if (trace)
                    fprintf(stderr, "received OACK <");
               /* tsize: funny, now we know the file size */
               if ((result = opt_get_tsize(data->tftp_options_reply)) > -1)
               {
                    if (trace)
                         fprintf(stderr, "tsize: %d, ", result);
               }
               /* timeout */
               if ((result = opt_get_timeout(data->tftp_options_reply)) > -1)
               {
                    if (trace)
                         fprintf(stderr, "timeout: %d, ", result);
               }
               /* blksize: resize the buffer please */
               if ((result = opt_get_blksize(data->tftp_options_reply)) > -1)
               {
                    if (trace)
                         fprintf(stderr, "blksize: %d, ", result);
                    data->data_buffer = realloc(data->data_buffer, result + 4);
                    tftphdr = (struct tftphdr *)data->data_buffer;
                    if (data->data_buffer == NULL)
                    {
                         fprintf(stderr, "tftp: memory allocation failure.\n");
                         exit(1);
                    }
                    data->data_buffer_size = result + 4;
               }
               /* multicast: yish, it's more complex. If we are a master,
                  we are responsible to ask packet with an ACK. If we are
                  not master, then just receive packets. Missing packets
                  will be asked when we become a master client */
               
               if (trace)
                    fprintf(stderr, "\b\b>\n");
               state = S_SEND_ACK;
               break;
          case S_DATA_RECEIVED:
               timeout_state = S_SEND_ACK;
               block_number = ntohs(tftphdr->th_block);
               if (trace)
                    fprintf(stderr, "received DATA <block: %d, size: %d>\n",
                            ntohs(tftphdr->th_block), data_size - 4);
               fseek(fp, (block_number - 1) * (data->data_buffer_size - 4),
                     SEEK_SET);
               if (fwrite(tftphdr->th_data, 1, data_size - 4, fp) !=
                   (data_size - 4))
               {
                    
                    fprintf(stderr, "tftp: error writing to file %s\n",
                            data->local_file);
                    tftp_send_error(sockfd, &sa, ENOSPACE,
                                    tftp_errmsg[ENOSPACE]);
                    state = S_END;
                    break;
               }
               if (data_size < data->data_buffer_size)
                    all_blocks_received = 1;
               else
                    all_blocks_received = 0;
               state = S_SEND_ACK;
               break;
          case S_END:
               if (fp)
                    fclose(fp);
               return OK;
               break;
          case S_ABORT:
               if (fp)
                    fclose(fp);
               fprintf(stderr, "tftp: aborting\n");
          default:
               return ERR;
          }
     }
}

/*
 * Send a file. This is implemented as a state machine using a while loop
 * and a switch statement. Function flow is as follow:
 *  - sanity check
 *  - enter state machine
 *
 *     1) send request
 *     2) wait replay
 *          - if ACK, goto 3
 *          - if OACK (option acknowledge) acknowledge this option, goto 2
 *          - if ERROR abort
 *          - if TIMEOUT goto previous state
 *     3) send data, goto 2
 */
int tftp_send_file(struct client_data *data)
{
     int state = S_SEND_REQ;    /* current state in the state machine */
     int timeout_state;         /* what state should we go on when timeout */
     int result;
     int block_number = 0;
     int data_size;             /* size of data received */
     int sockfd = data->sockfd; /* just to simplify calls */
     struct sockaddr_in sa;     /* a copy of data.sa_peer */
     int connected;             /* 1 when sockfd is connected */
     struct tftphdr *tftphdr = (struct tftphdr *)data->data_buffer;
     FILE *fp;                  /* the local file pointer */
     int number_of_timeout = 0;
     struct stat file_stat;
     int convert = 0;           /* if true, do netascii convertion */
     
     /* make sure the socket is not connected */
     sa.sin_family = AF_UNSPEC;
     connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
     connected = 0;

     /* copy sa_peer structure */
     memcpy(&sa, &data->sa_peer, sizeof(sa));

     /* check to see if conversion is requiered */
     if (strcasecmp(data->tftp_options[OPT_MODE].value, "netascii") == 0)
          convert = 1;

     /* make sure the data buffer is SEGSIZE + 4 bytes */
     if (data->data_buffer_size != (SEGSIZE + 4))
     {
          data->data_buffer = realloc(data->data_buffer, SEGSIZE + 4);
          tftphdr = (struct tftphdr *)data->data_buffer;
          if (data->data_buffer == NULL)
          {
               fprintf(stderr, "tftp: memory allocation failure.\n");
               exit(1);
          }
          data->data_buffer_size = SEGSIZE + 4;
     }

     /* open the file for reading */
     if ((fp = fopen(data->local_file, "r")) == NULL)
     {
          fprintf(stderr, "tftp: can't open %s for reading.\n",
                  data->local_file);
          return ERR;
     }

     /* When sending a file with the tsize argument, we shall
        put the file size as argument */
     fstat(fileno(fp), &file_stat);
     if (opt_get_tsize(data->tftp_options) > -1)
          opt_set_tsize(file_stat.st_size, data->tftp_options);
               
     while (1)
     {
          switch (state)
          {
          case S_SEND_REQ:
               timeout_state = S_SEND_REQ;
               if (trace)
                    fprintf(stderr, "sent WRQ <file: %s, mode: %s>\n",
                            data->tftp_options[OPT_FILENAME].value,
                            data->tftp_options[OPT_MODE].value);
               /* send request packet */
               if (tftp_send_request(sockfd, &sa, WRQ, data->data_buffer,
                                     data->data_buffer_size,
                                     data->tftp_options) == ERR)
                    state = S_ABORT;
               else
                    state = S_WAIT_PACKET;
               break;
          case S_SEND_DATA:
               timeout_state = S_SEND_DATA;
               if (feof(fp))
               {
                    state = S_END;
                    break;
               }
               fseek(fp, (block_number) * (data->data_buffer_size - 4),
                     SEEK_SET);
               data_size = fread(tftphdr->th_data, 1,
                                 data->data_buffer_size - 4, fp) + 4;
               if (trace)
                    fprintf(stderr, "sent DATA <block: %d, size: %d>\n",
                            block_number + 1, data_size - 4);
               tftp_send_data(sockfd, &sa, block_number + 1,
                              data_size, data->data_buffer);
               state = S_WAIT_PACKET;
               break;
          case S_WAIT_PACKET:
               data_size = data->data_buffer_size;
               result = tftp_get_packet(sockfd, &sa, data->timeout,
                                        &data_size, data->data_buffer);
               switch (result)
               {
               case GET_TIMEOUT:
                    number_of_timeout++;
                    fprintf(stderr, "timeout: retrying...\n");
                    if (number_of_timeout > NB_OF_RETRY)
                         state = S_ABORT;
                    else
                         state = timeout_state;
                    break;
               case GET_ACK:
                    /* if the socket if not connected, connect it */
                    if (!connected)
                    {
                         connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
                         connected = 1;
                    }
                    block_number = ntohs(tftphdr->th_block);
                    if (trace)
                         fprintf(stderr, "received ACK <block: %d>\n",
                                 block_number);
                    state = S_SEND_DATA;
                    break;
               case GET_OACK:
                    /* if the socket if not connected, connect it */
                    if (!connected)
                    {
                         connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
                         connected = 1;
                    }
                    state = S_OACK_RECEIVED;
                    break;
               case GET_ERROR:
                    fprintf(stderr, "tftp: error received from server <");
                    fwrite(tftphdr->th_msg, 1, data_size - 4, stderr);
                    fprintf(stderr, ">\n");
                    state = S_ABORT;
                    break;
               case GET_DISCARD:
                    fprintf(stderr, "ftpd: packet discard.\n");
                    break;
               case ERR:
                    fprintf(stderr, "tftp: unknown error.\n");
                    state = S_ABORT;
                    break;
               default:
                    fprintf(stderr, "tftp: abnormal return value %d.\n",
                            result);
               }
               break;
          case S_OACK_RECEIVED:
               /* clean the tftp_options structure */
               memcpy(data->tftp_options_reply, tftp_default_options,
                      sizeof(tftp_default_options));
               /*
                * look in the returned string for tsize, timeout, blksize or
                * multicast
                */
               opt_disable_options(data->tftp_options_reply, NULL);
               opt_parse_options(data->data_buffer, data_size,
                                 data->tftp_options_reply);
               if (trace)
                    fprintf(stderr, "received OACK <");
               /* tsize: funny, now we know the file size */
               if ((result = opt_get_tsize(data->tftp_options_reply)) > -1)
               {
                    if (trace)
                         fprintf(stderr, "tsize: %d, ", result);
               }
               /* timeout */
               if ((result = opt_get_timeout(data->tftp_options_reply)) > -1)
               {
                    if (trace)
                         fprintf(stderr, "timeout: %d, ", result);
               }
               /* blksize: resize the buffer please */
               if ((result = opt_get_blksize(data->tftp_options_reply)) > -1)
               {
                    if (trace)
                         fprintf(stderr, "blksize: %d, ", result);
                    data->data_buffer = realloc(data->data_buffer, result + 4);
                    tftphdr = (struct tftphdr *)data->data_buffer;
                    if (data->data_buffer == NULL)
                    {
                         fprintf(stderr, "tftp: memory allocation failure.\n");
                         exit(1);
                    }
                    data->data_buffer_size = result + 4;
               }
               /* multicast: yish, it's more complex. If we are a master,
                  we are responsible to ask packet with an ACK. If we are
                  not master, then just receive packets. Missing packets
                  will be asked when we become a master client */
               
               if (trace)
                    fprintf(stderr, "\b\b>\n");
               state = S_SEND_DATA;
               break;
          case S_END:
               if (fp)
                    fclose(fp);
               return OK;
               break;
          case S_ABORT:
               if (fp)
                    fclose(fp);
               fprintf(stderr, "tftp: aborting\n");
          default:             
               return ERR;             
          }
     }
}
