/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftpd.h
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

#ifndef tftpd_h
#define tftpd_h

#include <pthread.h>
#include <arpa/tftp.h>
#include <arpa/inet.h>
#include "tftp_io.h"

/*
 * Per thread data. There is a thread for each client or group
 * (multicast) of client.
 */
struct thread_data {
     pthread_t tid;

     /* private to thread */
     char *data_buffer;
     int data_buffer_size;
     char *ack_buffer;
     int ack_buffer_size;
     int timeout;
     int sockfd;
     struct sockaddr_in sa_in;

     /*
      * Must be locked by self when writing, and by other when reading.
      * Other thread have only read access.
      */
     pthread_mutex_t opt_mutex;
     struct tftp_opt *tftp_options;
 
     /* 
      * Must lock to insert in the list, but to to read or write, since only
      * the propriotary thread do it.
      */
     pthread_mutex_t client_mutex;
     struct client_info *client_info;

     /* must be lock (list lock) to update */
     struct thread_data *prev;
     struct thread_data *next;
};

struct client_info {
     struct sockaddr_in client;
     int master_client;
     int done;                  /* that client as receive it's file */
     struct client_info *next;
};

/*
 * Functions defined in tftpd_file.c
 */
int tftpd_receive_file(struct thread_data *data);
int tftpd_send_file(struct thread_data *data);

/*
 * Defined in tftpd_list.c, operation on thread_data list.
 */
int tftpd_list_add(struct thread_data *new);
int tftpd_list_remove(struct thread_data *old);
int tftpd_list_num_of_thread(void);
int tftpd_list_find_multicast_server(struct thread_data **thread,
                                     struct tftp_opt *tftp_options);
/*
 * Defined in tftpd_list.c, operation on client structure list.
 */
void tftpd_clientlist_add(struct client_info *head,
                          struct client_info *client);
void tftpd_clientlist_remove(struct client_info *head,
                             struct client_info *client);
void tftpd_clientlist_free(struct client_info *head);

/* read only variables unless for the main thread, at initialisation */
extern char directory[MAXLEN];

#endif
