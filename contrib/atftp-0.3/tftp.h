/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftp.h
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

#ifndef tftp_h
#define tftp_h

#include "tftp_def.h"

struct client_data {
     char *data_buffer;         /* used for sending and receiving of data */
     int data_buffer_size;      /* size of the buffer, may be reallocated */

     char local_file[VAL_SIZE]; /* the file we are reading or writing is not
                                   necessary the same on the server */
     struct tftp_opt *tftp_options; /* hold requested options */
     struct tftp_opt *tftp_options_reply; /* hold server reply */
     int timeout;               /* client side timeout for select() */

     char hostname[MAXLEN];     /* peer's hostname */
     short port;                /* tftp port for the server, 69 by default */

     struct servent *sp;        /* server entry for tftp service */
     struct sockaddr_in sa_peer; /* peer address and port */
     struct sockaddr_in sa_local; /* local address and port */
     int sockfd;

     struct sockaddr_in sa_mcast;
     int master_client;

     int connected;             /* we are 'connected' */
};

extern int trace;
extern int verbose;

#endif
