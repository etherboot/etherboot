/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftpd_list.c
 *    thread_data and client list related functions
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
#include "tftpd.h"

/*
 * thread_data is a double link list of server threads. Server threads
 * are started by the main thread when a new request arrives. The number
 * of threads running is held in number_of_thread. Any access to the
 * thread_data list: insertion and extraction of elements must be protected
 * by the mutex thread_list_mutex. Note that individual threads do not need to
 * lock the list when playing in their data. See tftpd.h. You must lock when
 * updating.
 */
struct thread_data *thread_data = NULL;
static int number_of_thread = 0;
pthread_mutex_t thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Add a new thread_data structure to the list. You must lock the mutex
 * before calling this function and release it after.
 */
int tftpd_list_add(struct thread_data *new)
{
     struct thread_data *current = thread_data;
     int ret;

     pthread_mutex_lock(&thread_list_mutex);

     number_of_thread++;
     ret = number_of_thread;

     if (thread_data == NULL)
     {
          thread_data = new;
          new->prev = NULL;
          new->next = NULL;
     }
     else
     {
          while (current->next != NULL)
               current = current->next;
          current->next = new;
          new->prev = current;
          new->next = NULL;
     }
     pthread_mutex_unlock(&thread_list_mutex);
     return ret;
}

/*
 * Remove a thread_data structure from the list. You must lock the mutex
 * before calling this function and release it after.
 */
int tftpd_list_remove(struct thread_data *old)
{
     struct thread_data *current = thread_data;
     int ret;

     pthread_mutex_lock(&thread_list_mutex);
     number_of_thread--;
     ret = number_of_thread;
    
     if (thread_data == old)
     {
          if (thread_data->next != NULL)
          {
               thread_data = thread_data->next;
               thread_data->prev = NULL;
          }
          else
               thread_data = NULL;
     }
     else
     {
          while (current != old)
               current = current->next;
          if (current->next && current->prev)
          {
               current->prev->next = current->next;
               current->next->prev = current->prev;
          }
          else
               current->prev->next = NULL;
     }
     pthread_mutex_unlock(&thread_list_mutex);
     return ret;
}

/*
 * Return the number of threads actually started.
 */
int tftpd_list_num_of_thread(void)
{
     int ret;
     pthread_mutex_lock(&thread_list_mutex);
     ret = number_of_thread;
     pthread_mutex_unlock(&thread_list_mutex);
     return ret;
}

/*
 * This function looks for a thread serving exactly the same
 * file and with the same options as another client. This implies a
 * multicast enabled client. The calling function must lock the
 * thread_list_mutex.
 */
int tftpd_list_find_multicast_server(struct thread_data **thread,
                                     struct tftp_opt *tftp_options)
{
/*
  struct thread_data *current = thread_data;
  char options[MAXLEN];
  char string[MAXLEN];

  *thread = NULL;
  opt_request_to_string(tftp_options, options, MAXLEN);

  while (current)
  {
  pthread_mutex_lock(&current->opt_mutex);
  opt_request_to_string(current->tftp_options, string, MAXLEN);
  pthread_mutex_unlock(&current->opt_mutex);
  if (strncmp(string, options, MAXLEN) == 0)
  {
  *thread = current;
  return 1;
  }
  current = current->next;
  }
*/
     return 0;
}

/*
 * Add an element to the client list. Note that the first element,
 * the head, is always allocated when creating a server thread. So, a call
 * to this function is required for multicast only.
 */
void tftpd_clientlist_add(struct client_info *head,
                          struct client_info *client)
{
     struct client_info *tmp = head;
     /* insert the new client at the end */
     while (tmp->next != NULL)
          tmp = tmp->next;
     tmp->next = client;
}

/*
 * As of the the above comment, we never remove the head element.
 */
void tftpd_clientlist_remove(struct client_info *head,
                             struct client_info *client)
{
     struct client_info *tmp = head;

     while ((tmp->next != client) && (tmp->next != NULL))
          tmp = tmp->next;
     if (tmp->next == NULL)
          return;
     tmp->next = tmp->next->next;
}

/*
 * Free all allocated client structure.
 */
void tftpd_clientlist_free(struct client_info *head)
{
     struct client_info *tmp;

     while (head)
     {
          tmp = head;
          head = head->next;
          free(tmp);
     }
}
