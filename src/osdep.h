#ifndef	__OSDEP_H__
#define __OSDEP_H__

#define __unused __attribute__((unused))

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#define	__LITTLE_ENDIAN		/* x86 */

/* Taken from /usr/include/linux/hfs_sysdep.h */
#if defined(__BIG_ENDIAN)
#	if !defined(__constant_htonl)
#		define __constant_htonl(x) (x)
#	endif
#	if !defined(__constant_htons)
#		define __constant_htons(x) (x)
#	endif
#elif defined(__LITTLE_ENDIAN)
#	if !defined(__constant_htonl)
#		define __constant_htonl(x) \
        ((unsigned long int)((((unsigned long int)(x) & 0x000000ffU) << 24) | \
                             (((unsigned long int)(x) & 0x0000ff00U) <<  8) | \
                             (((unsigned long int)(x) & 0x00ff0000U) >>  8) | \
                             (((unsigned long int)(x) & 0xff000000U) >> 24)))
#	endif
#	if !defined(__constant_htons)
#		define __constant_htons(x) \
        ((unsigned short int)((((unsigned short int)(x) & 0x00ff) << 8) | \
                              (((unsigned short int)(x) & 0xff00) >> 8)))
#	endif
#else
#	error "Don't know if bytes are big- or little-endian!"
#endif

#define ntohl(x) \
(__builtin_constant_p(x) ? \
 __constant_htonl((x)) : \
 __swap32(x))
#define htonl(x) \
(__builtin_constant_p(x) ? \
 __constant_htonl((x)) : \
 __swap32(x))
#define ntohs(x) \
(__builtin_constant_p(x) ? \
 __constant_htons((x)) : \
 __swap16(x))
#define htons(x) \
(__builtin_constant_p(x) ? \
 __constant_htons((x)) : \
 __swap16(x))

static inline unsigned long int __swap32(unsigned long int x)
{
	__asm__("xchgb %b0,%h0\n\t"
		"rorl $16,%0\n\t"
		"xchgb %b0,%h0"
		: "=q" (x)
		: "0" (x));
	return x;
}

static inline unsigned short int __swap16(unsigned short int x)
{
	__asm__("xchgb %b0,%h0"
		: "=q" (x)
		: "0" (x));
	return x;
}

/* Make routines available to all */
#define	swap32(x)	__swap32(x)
#define	swap16(x)	__swap16(x)

/* We should not depend on any system header files except possibly
 * compiler supplied std c headers.  Copy headers if you need them.
 */
#if defined(FREEBSD_PXEEMU) && 0
#undef	htonl
#undef	htons
#undef	ntohl
#undef	ntohs
#include <sys/types.h>
#include "/sys/boot/i386/libi386/pxe.h"
#include "/sys/boot/i386/btx/lib/btxv86.h"
#endif

/* Endian defines */
#define cpu_to_le32(x) (x)
#define cpu_to_le16(x) (x)
#define cpu_to_be32(x) (htonl(x))
#define cpu_to_be16(x) (htons(x))
#define le32_to_cpu(x) (x)
#define le16_to_cpu(x) (x)
#define be32_to_cpu(x) (ntohl(x))
#define be16_to_cpu(x) (ntohs(x))


#include "stdint.h"
#include "linux-asm-string.h"
#include "linux-asm-io.h"


#define ULONG_MAX 0xffffffffUL
/* within 1MB of ULONG_MAX is too close */
#define MAX_ADDR (ULONG_MAX - (1024*1024) +1)

typedef	unsigned long Address;

/* ANSI prototyping macro */
#ifdef	__STDC__
#define	P(x)	x
#else
#define	P(x)	()
#endif

#endif

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
