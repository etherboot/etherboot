#ifndef	ETHERBOOT16
/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:34:26  rpd
 *	$Id$
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * Serial bootblock interface routines
 * Copyright (c) 1994, J"org Wunsch
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

#if	defined(CONSOLE_SERIAL) || defined(CONSOLE_DUAL)

	.file	"serial.S"

	.text

/*
 * The serial port interface routines implement a simple polled i/o
 * interface to a standard serial port.  Due to the space restrictions
 * for the boot blocks, no BIOS support is used (since BIOS requires
 * expensive real/protected mode switches), instead the rudimentary
 * BIOS support is duplicated here.
 *
 * The base address and speed for the i/o port are passed from the
 * Makefile in the COMCONSOLE and CONSPEED preprocessor macros.  The
 * line control parameters are currently hard-coded to 8 bits, no
 * parity, 1 stop bit (8N1).  This can be changed in init_serial().
 */

/*
 * void serial_putc(int ch);
 *	Write character `ch' to port COMCONSOLE.
 */
	.globl  serial_putc
serial_putc:
	cmpb	$0,found
	je	2f		# no serial interface
	movl	$10000, %ecx	# timeout
	movl	$COMCONSOLE + 5, %edx	# line status reg
1:
	decl	%ecx
	je	2f
	inb	%dx, %al
	testb	$0x20, %al
	je	1b		# TX buffer not empty

	movb	4(%esp), %al

	subl	$5, %edx	# TX output reg
	outb	%al, %dx	# send this one
2:
	ret

/*
 * int serial_getc(void);
 *	Read a character from port COMCONSOLE.
 */
	.globl serial_getc
serial_getc:
	mov	$COMCONSOLE + 5, %edx	# line status reg
1:
	inb	%dx, %al
	testb	$0x01, %al
	je	1b		# no rx char available

	xorl	%eax, %eax
	subl	$5, %edx	# rx buffer reg
	inb	%dx, %al	# fetch (first) character

	andb	$0x7F, %al	# remove any parity bits we get
	cmpb	$0x7F, %al	# make DEL...
	jne	2f
	movb	$0x08, %al	# look like BS
2:
	ret

/*
 * int serial_ischar(void);
 *       If there is a character in the input buffer of port COMCONSOLE,
 *       return nonzero; otherwise return 0.
 */

	.globl serial_ischar
serial_ischar:
	xorl	%eax, %eax
	cmpb	$0,found
	je	1f
	movl	$COMCONSOLE + 5, %edx	# line status reg
	inb	%dx, %al
	andb	$0x01, %al		# rx char available?
1:
	ret

#if	!defined(COMBRD) && defined(CONSPEED)
/* Recent GNU as versions with ELF output format define / as a comment
 * character, because some misguided spec says so.  Do it the easy way and
 * just check for the usual values.  This is only compiled by gcc, so
 * #elif can be used (bcc doesn't understand it).  */
#if	(CONSPEED == 115200)
#define COMBRD 1
#elif	(CONSPEED == 57600)
#define COMBRD 2
#elif	(CONSPEED == 38400)
#define COMBRD 3
#elif	(CONSPEED == 19200)
#define COMBRD 6
#elif	(CONSPEED == 9600)
#define COMBRD 12
#elif	(CONSPEED == 2400)
#define COMBRD 48
#elif	(CONSPEED == 1200)
#define COMBRD 96
#elif	(CONSPEED == 300)
#define COMBRD 384
#else
#error Add your unusual baud rate to the table in serial.S!
#define	COMBRD	(115200 / CONSPEED)
#endif
#endif

#ifndef	COMPARM
#define	COMPARM	0x03
#endif

/*
 * int init_serial(void);
 *	Initialize port COMCONSOLE to speed CONSPEED, line settings 8N1.
 */
	.globl serial_init
serial_init:
#ifndef COMPRESERVE
	movl	$COMCONSOLE + 3, %edx	# line control reg
	movb	$0x80, %al
	outb	%al, %dx	# enable DLAB

	subl	$3, %edx	# divisor latch, low byte
	movb	$COMBRD & 0xff, %al
	outb	%al, %dx
	inb	%dx, %al	# read back to check if device present
	cmpb	$COMBRD & 0xff, %al
	je	2f
1:	xorl	%eax, %eax
	jmp	4f
2:
	incl	%edx		# divisor latch, high byte
	movb	$COMBRD >> 8, %al
	outb	%al, %dx
	inb	%dx, %al	# read back to check if device present
	cmpb	$COMBRD >> 8, %al
	jne	1b		# not present
	incl	%edx		# fifo control register (if any)
	xorl	%eax,%eax
	outb	%al, %dx	# disable fifo to reduce worst-case busy-wait

	incl	%edx		# line control reg
	movb	$COMPARM, %al
	outb	%al, %dx	# 8N1

	incl	%edx		# modem control reg
	outb	%al, %dx	# enable DTR/RTS
#endif
	/* Flush the input buffer. */
	movl	$COMCONSOLE + 5, %edx	# line status reg
3:
	subl	$5, %edx	# rx buffer reg
	inb	%dx, %al	# throw away (unconditionally the first time)
	addl	$5, %edx	# line status reg
	inb	%dx, %al
	testb	$0x01, %al
	jne	3b		# more

	movl	$1, %eax	# return true
4:	movb	%al,found
	ret

.lcomm	found,1

#endif
#endif
