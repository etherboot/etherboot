/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftp_def.h
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

#ifndef tftp_def_h
#define tftp_def_h

/* standard return value */
#define OK            0
#define ERR          -1

#define MAXLEN      256
#define TIMEOUT       5         /* Client timeout */
#define S_TIMEOUT     5         /* Server timout. */
#define NB_OF_RETRY   5

/* definition to use tftp_options structure */
#define OPT_FILENAME  0
#define OPT_MODE      1
#define OPT_TSIZE     2
#define OPT_TIMEOUT   3
#define OPT_BLKSIZE   4
#define OPT_MULTICAST 5
#define OPT_NUMBER    7

#define OPT_SIZE     12
#define VAL_SIZE     64

/* Structure definition for tftp options. */
struct tftp_opt {
     char option[OPT_SIZE];
     char value[VAL_SIZE];
     int specified;             /* specified by the client (for tftp server) */
     int enabled;               /* enabled for use by server or client */
};

extern struct tftp_opt tftp_default_options[OPT_NUMBER];
extern char *tftp_errmsg[9];

#endif
