/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftp_def.c
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

#include "tftp_def.h"

/*
 * This is the default option structure, that must be used
 * for initialisation.
 */

// FIXME: is there a way to use TIMEOUT and SEGSIZE here?
struct tftp_opt tftp_default_options[OPT_NUMBER] = {
     { "filename", "", 0, 1},   /* file to transfer */
     { "mode", "octet", 0, 1},  /* mode for transfer */
     { "tsize", "0", 0, 1 },    /* RFC1350 options. See RFC2347, */
     { "timeout", "5", 0, 1 },  /* 2348, 2349, 2090.  */
     { "blksize", "512", 0, 1 }, /* This is the default option */
     { "multicast", "", 0, 1 }, /* structure */
     { "", "", 0, 0}
};

/* Error message defined in RFC1350. */
char *tftp_errmsg[9] = {
     "Undefined error code",
     "File not found",
     "Access violation",
     "Disk full or allocation exceeded",
     "Illegal TFTP operation",
     "Unknown transfer ID",
     "File already exists",
     "No such user",
     "Failure to negotiate RFC1782 options",
};
