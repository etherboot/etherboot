/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * options.c
 *    Set of functions to deal with the options structure and for parsing
 *    options in TFTP data buffer.
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
#include <unistd.h>
#include <syslog.h>
#include <argz.h>
#include <arpa/tftp.h>
#include <string.h>
#include "options.h"

/*
 * Fill a structure with the request packet of the client.
 */
int opt_parse_request(char *data, int data_size, struct tftp_opt *options)
{
     char *entry = NULL;
     char *tmp;
     struct tftphdr *tftp_data = (struct tftphdr *)data;
     size_t size = data_size - sizeof(tftp_data->th_opcode);

     /* read filename */
     entry = argz_next(tftp_data->th_stuff, size, entry);
     if (!entry)
          return ERR;
     else
          opt_set_options(options, "filename", entry);
     /* read mode */
     entry = argz_next(tftp_data->th_stuff, size, entry);
     if (!entry)
          return ERR;
     else
          opt_set_options(options, "mode", entry);
     /* scan for options */
     // FIXME: we should use opt_parse_options() here
     while ((entry = argz_next(tftp_data->th_stuff, size, entry)))
     {
          tmp = entry;
          entry = argz_next(tftp_data->th_stuff, size, entry);
          if (!entry)
               return ERR;
          else
               opt_set_options(options, tmp, entry);
     }
     return OK;
}

/*
 * Fill a structure looking only at TFTP options.
 */
int opt_parse_options(char *data, int data_size, struct tftp_opt *options)
{
     char *entry = NULL;
     char *tmp;
     struct tftphdr *tftp_data = (struct tftphdr *)data;
     size_t size = data_size - sizeof(tftp_data->th_opcode);

     while ((entry = argz_next(tftp_data->th_stuff, size, entry)))
     {
          tmp = entry;
          entry = argz_next(tftp_data->th_stuff, size, entry);
          if (!entry)
               return ERR;
          else
               opt_set_options(options, tmp, entry);
     }
     return OK;
}

/*
 * Set an option by name in the structure.
 * name is the name of the option as in tftp_def.c.
 * name is it's new value, that must comply with the rfc's.
 * When setting an option, it is marked as specified.
 * 
 */
int opt_set_options(struct tftp_opt *options, char *name, char *value)
{
     int i;

     for (i = 0; i < OPT_NUMBER; i++)
     {
          if (strcasecmp(name, options[i].option) == 0)
          {
               options[i].specified = 1;
               if (value)
                    strncpy(options[i].value, value, VAL_SIZE);
               else
                    strncpy(options[i].value, tftp_default_options[i].value,
                            VAL_SIZE);
               return OK;
          }
     }
     return ERR;
}

/*
 * Disable an option by name.
 */
int opt_disable_options(struct tftp_opt *options, char *name)
{
     int i;

     for (i = 2; i < OPT_NUMBER; i++)
     {
          if (name == NULL)
               options[i].specified = 0;
          else
          {
               if (strcasecmp(name, options[i].option) == 0)
               {
                    options[i].specified = 0;
                    return OK;
               }
          }
     }
     if (name == NULL)
          return OK;
     return ERR;
}


/*
 * Return 1 if one or more options are specified in the options structure.
 */
int opt_support_options(struct tftp_opt *options)
{
     int i;
     int support = 0;

     for (i = 2; i < OPT_NUMBER; i++)
     {
          if (options[i].specified)
               support = 1;
     }
     return support;
}

/*
 * The next few functions deal with TFTP options. Function's name are self
 * explicative.
 */

int opt_get_tsize(struct tftp_opt *options)
{
     int tsize;
     if (options[OPT_TSIZE].enabled && options[OPT_TSIZE].specified)
     {
          tsize = atoi(options[OPT_TSIZE].value);
          return tsize;
     }
     return -1;
}

int opt_get_timeout(struct tftp_opt *options)
{
     int timeout;
     if (options[OPT_TIMEOUT].enabled && options[OPT_TIMEOUT].specified)
     {
          timeout = atoi(options[OPT_TIMEOUT].value);
          return timeout;
     }
     return -1;
}

int opt_get_blksize(struct tftp_opt *options)
{
     int blksize;
     if (options[OPT_BLKSIZE].enabled && options[OPT_BLKSIZE].specified)
     {
          blksize = atoi(options[OPT_BLKSIZE].value);
          return blksize;
     }
     return -1;
}

void opt_set_tsize(int tsize, struct tftp_opt *options)
{
     snprintf(options[OPT_TSIZE].value, VAL_SIZE, "%d", tsize);
}

void opt_set_timeout(int timeout, struct tftp_opt *options)
{
     snprintf(options[OPT_TIMEOUT].value, VAL_SIZE, "%d", timeout);
}

void opt_set_blksize(int blksize, struct tftp_opt *options)
{
     snprintf(options[OPT_BLKSIZE].value, VAL_SIZE, "%d", blksize);
}

/*
 * Format the content of the options structure (this is the content of a
 * read or write request) to a null separated data string corresponding to
 * the TFTP data format for read and write request.
 */
void opt_request_to_string(struct tftp_opt *options, char *string, int len)
{
     int i, index = 0;

     for (i = 0; i < 2; i++)
     {
          strncpy(string + index, options[i].option, OPT_SIZE);
          index += strlen(options[i].option);
          strcpy(string + index, ": ");
          index += 2;
          strncpy(string + index, options[i].value, VAL_SIZE);
          index += strlen(options[i].value);
          strcpy(string + index, ", ");
          index += 2;
     }
     opt_options_to_string(options, string + index, len - index);
}

/*
 * Convert an entry of the options structure to a null separated data
 * string.
 */
void opt_options_to_string(struct tftp_opt *options, char *string, int len)
{
     int i, index = 0;

     //FIXME: should check len to prevent overflow...

     for (i = 2; i < OPT_NUMBER; i++)
     {
          if (options[i].specified && options[i].enabled)
          {
               strncpy(string + index, options[i].option, OPT_SIZE);
               index += strlen(options[i].option);
               strcpy(string + index, ": ");
               index += 2;
               strncpy(string + index, options[i].value, VAL_SIZE);
               index += strlen(options[i].value);
               strcpy(string + index, ", ");
               index += 2;
          }
          string[index -2] = 0;
     }     
}
