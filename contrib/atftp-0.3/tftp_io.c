/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftp_io.c
 *    I/O operation routines common to both client and server
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
#include <unistd.h>
#include <arpa/inet.h>
#include <arpa/tftp.h>
#include "string.h"
#include "tftp_io.h"

/*
 *  2 bytes   string    1 byte  string  1 byte  string 1 byte  string
 * --------------------------------------------------------------------->
 *| Opcode  | Filename |   0   | Mode  |   0   | Opt1 |   0   | Value1 <
 * --------------------------------------------------------------------->
 *
 *    string  1 byte  string  1 byte
 *  >--------------------------------
 * <  OptN  |   0   | ValueN |   0   |
 *  >--------------------------------
 */
int tftp_send_request(int socket, struct sockaddr_in *sa, short type,
                      char *data_buffer, int data_buffer_size,
                      struct tftp_opt *tftp_options)
{
     int i;
     int result;
     int buf_index = 0;
     struct tftphdr *tftphdr = (struct tftphdr *)data_buffer;
     char *filename = tftp_options[OPT_FILENAME].value;
     char *mode = tftp_options[OPT_MODE].value;

     /* write the opcode */
     tftphdr->th_opcode = htons(type);
     buf_index += 2;
     /* write file name */
     strcpy(data_buffer + buf_index, filename);
     buf_index += strlen(filename);
     buf_index++;
     strcpy(data_buffer + buf_index, mode);
     buf_index += strlen(mode);
     buf_index++;
     
     for (i = 2; ; i++)
     {
          if (strlen(tftp_options[i].option) == 0)
               break;
          if (tftp_options[i].enabled && tftp_options[i].specified)
          {
               strcpy(data_buffer + buf_index, tftp_options[i].option);
               buf_index += strlen(tftp_options[i].option);
               buf_index++;    
               strcpy(data_buffer + buf_index, tftp_options[i].value);
               buf_index += strlen(tftp_options[i].value);
               buf_index++;    
          }
     }
     /* send the buffer */
     result = sendto(socket, data_buffer, buf_index, 0,
                     (struct sockaddr *)sa, sizeof(*sa));
     if (result < 0)
          return ERR;
     return OK;
}

/*
 *  2 bytes   2 bytes
 * -------------------
 *| Opcode  | Block # |
 * -------------------
 */
int tftp_send_ack(int socket, struct sockaddr_in *sa, short block_number)
{
     struct tftphdr tftphdr;
     int result;

     tftphdr.th_opcode = htons(ACK);
     tftphdr.th_block = htons(block_number);

     result = sendto(socket, &tftphdr, 4, 0, (struct sockaddr *)sa,
                     sizeof(*sa));
     if (result < 0)
          return ERR;
     return OK;
}

/*
 *  2 bytes   string  1 byte  string  1 byte  string  1 byte   string  1 byte
 * ---------------------------------------------------------------------------
 *| Opcode  | Opt1  |   0   | Value1 |   0   | OptN  |   0   | ValueN |   0   |
 * ---------------------------------------------------------------------------
 */
int tftp_send_oack(int socket, struct sockaddr_in *sa, 
                   struct tftp_opt *tftp_options)
{
     
     int i;
     int result;
     int index = 0;
     char buffer[256];
     struct tftphdr *tftphdr = (struct tftphdr *)buffer;

     /* write the opcode */
     tftphdr->th_opcode = htons(OACK);
     index += 2;
     
     for (i = 2; i < OPT_NUMBER; i++)
     {
          if (tftp_options[i].enabled && tftp_options[i].specified)
          {
               strcpy(buffer + index, tftp_options[i].option);
               index += strlen(tftp_options[i].option);
               index++;
               strcpy(buffer + index, tftp_options[i].value);
               index += strlen(tftp_options[i].value);
               index++;    
          }
     }
     /* send the buffer */
     result = sendto(socket, buffer, index, 0, (struct sockaddr *)sa,
                     sizeof(*sa));
     if (result < 0)
          return ERR;
     return OK;
}

/*
 *  2 bytes   2 bytes     string   1 byte
 * ---------------------------------------
 *| Opcode  | ErrorCode | ErrMsg |    0   |
 * ---------------------------------------
 */
int tftp_send_error(int socket, struct sockaddr_in *sa,
                    short err_code, char *err_msg)
{
     char buffer[256];
     int size;
     int result;
     struct tftphdr *tftphdr = (struct tftphdr *)buffer;

     if (err_code > EOPTNEG)
          return ERR;
     tftphdr->th_opcode = htons(ERROR);
     tftphdr->th_code = htons(err_code);
     strncpy(tftphdr->th_msg, tftp_errmsg[err_code],
             strlen(tftp_errmsg[err_code]));

     size = 4 + strlen(tftp_errmsg[err_code]);

     result = sendto(socket, tftphdr, size, 0, (struct sockaddr *)sa,
                     sizeof(*sa));
     if (result < 0)
          return ERR;
     return OK;
}

/*
 *  2 bytes   2 bytes   N bytes
 * ----------------------------
 *| Opcode  | Block # | Data   |
 * ----------------------------
 */
int tftp_send_data(int socket, struct sockaddr_in *sa, short block_number,
                   int size, char *data)
{
     struct tftphdr *tftphdr = (struct tftphdr *)data;
     int result;

     tftphdr->th_opcode = htons(DATA);
     tftphdr->th_block = htons(block_number);

     result = sendto(socket, data, size, 0, (struct sockaddr *)sa,
                     sizeof(*sa));
     if (result < 0)
          return ERR;
     return OK;
}

/*
 * Wait for a packet.
 */
int tftp_get_packet(int socket, struct sockaddr_in *sa, int timeout,
                    int *size, char *data)
{
     int result;
     struct timeval tv;
     fd_set rfds;
     struct sockaddr_in from;
     socklen_t fromlen = sizeof(from);
     struct tftphdr *tftphdr = (struct tftphdr *)data;

     memset(&from, 0, sizeof(from));

     /* Wait up to five seconds. */
     tv.tv_sec = timeout;
     tv.tv_usec = 0;

     /* Watch socket to see when it has input. */
     FD_ZERO(&rfds);
     FD_SET(socket, &rfds);

     if ((result = select(FD_SETSIZE, &rfds, NULL, NULL, &tv)) < 0)
          return ERR;
     else
     {
          if (result == 0)
               return GET_TIMEOUT;
          if (result == 1)
          {
               /* read the data */
               result = recvfrom(socket, data, *size, 0,
                                 (struct sockaddr *)&from, &fromlen);
               if (result <= 0)
                    return ERR;

               *size = result;
               
               /* if sa as never been initialised, sa->sin_port is still 0 */
               if (sa->sin_port == htons(0))
                    memcpy(sa, &from, sizeof(from));           
               /* if sa is initialised, and sa->sin_port = 69, put in the
                  new port number */
               else if (sa->sin_port == htons(69))
                    sa->sin_port = from.sin_port;
               else
                    /* verify that the reply come from the remote host */
                    if (sa->sin_port != from.sin_port)
                         /* discard the packet */
                         return GET_DISCARD;
               switch (ntohs(tftphdr->th_opcode))
               {
               case RRQ:
                    return GET_RRQ;
               case WRQ:
                    return GET_WRQ;
               case ACK:
                    return GET_ACK;
               case OACK:
                    return GET_OACK;
               case ERROR:
                    return GET_ERROR;
               case DATA:
                    return GET_DATA;
               default:
                    return ERR;
               }
          }
     }
     return OK;
}
