/*
	Copyright (C) 2000, Entity Cyber, Inc.

	Authors: Gary Byers (gb@thinguin.org)
		 Marty Connor (mdc@thinguin.org)

	This software may be used and distributed according to the terms
	of the GNU Public License (GPL), incorporated herein by reference.

	Description:	
	
	This is just a little bit of code and data that can get prepended
	to an Etherboot ROM image in order to allow LILO to load the
	result as if it were a Linux kernel image.

	A real Linux kernel image consists of a one-sector boot loader
	(to load the image from a floppy disk), followed a few sectors
	of setup code, followed by the kernel code itself.  There's
	a table in the first sector (starting at offset 497) that indicates
	how many sectors of setup code follow the first sector and which
	contains some other parameters that aren't interesting in this
	case.

	When LILO loads the sectors that comprise a kernel image, it doesn't
	execute the code in the first sector (since that code would try to
	load the image from a floppy disk.)  The code in the first sector
	below doesn't expect to get executed (and prints an error message
	if it ever -is- executed.)  LILO's only interested in knowing the
	number of setup sectors advertised in the table (at offset 497 in
	the first sector.)

	Etherboot doesn't require much in the way of setup code.
	Historically, the Linux kernel required at least 4 sectors of
	setup code.  Current versions of LILO look at the byte at
	offset 497 in the first sector to indicate how many sectors
	of setup code are contained in the image.

*/

#include <asm/boot.h>

#define SYSSIZE 2048		/* 2048 * 16 bytes = 32kB maximum size of
				   .ROM file */
#define SETUPSECS 1		/* Minimal nr of setup-sectors */
#define BOOTSEG 0x07C0		/* original address of boot-sector */
#define INITSEG DEF_INITSEG	/* we move boot here - out of the way */
#define SETUPSEG DEF_SETUPSEG	/* setup starts here */
#define SYSSEG DEF_SYSSEG	/* system loaded at 0x10000 (65536). */

#ifdef	USE_AS86
#define BYTE(x) .byte x
#define WORD(x) .word x
#define	CON(x)	*x
#define	BCON(x)	*x
#define	LOC(x)	x
#define	WLOC(x)	x
#define	STRDECL(x)	.ascii	x
#define	JMP	jmp
#define	SEG_ES	seg	es
#define ORG(x)	org	x
	.text
#endif
#ifdef	USE_NASM
#define BYTE(x) db x
#define WORD(x) dw x
#define	CON(x)	x
#define	BCON(x)	byte x
#define	LOC(x)	[x]
#define	WLOC(x)	word [x]
#define	STRDECL(x)	db	x
#define	JMP	jmp short
#define	SEG_ES	es
#define	ORG(x)	times x-($-$$) db 0
	section	.text
#endif
	org	0
	
/* 
	This is a minimal boot sector.  If anyone tries to execute it (e.g., if
	a .lilo file is dd'ed to a floppy), print an error message. 
*/
	
bootsector:
	jmp	BOOTSEG:go	/* reload cs:ip to match relocation addr */
go:
	mov	di,CON(0x2000)	/*  0x2000 is arbitrary value >= length
				    of bootsect + room for stack */

	mov	ax,CON(BOOTSEG)
	mov	ds,ax
	mov	es,ax
	
	cli
	mov	ss,ax		/* put stack at BOOTSEG:0x2000. */
	mov	sp,di
	sti
	
	mov	cx,CON(why_end-why)
	mov	si,CON(why)

	mov	bx,CON(0x0007)	/* page 0, attribute 7 (normal) */
	mov	ah,CON(0x0e)	/* write char, tty mode */
prloop:
	lodsb
	int	0x10
	loop	prloop
freeze:	JMP	freeze
	
why:	STRDECL('This image cannot be loaded from a floppy disk.')
	BYTE(13)
	BYTE(10)
why_end:


	ORG(497)
setup_sects:
	BYTE(SETUPSECS)
root_flags:
	WORD(0)
syssize:
	WORD(SYSSIZE)
swap_dev:
	WORD(0)
ram_size:
	WORD(0)
vid_mode:
	WORD(0)
root_dev:
	WORD(0)
boot_flag:
	WORD(0xAA55)

/*
	We're now at the beginning of the second sector of the image -
	where the setup code goes.

	We don't need to do too much setup for Etherboot.

	This code gets loaded at SETUPSEG:0.  It wants to start
	executing the Etherboot image that's loaded at SYSSEG:0 and
	whose entry point is SYSSEG:6.

	Before jumping to the image, check its signature.
*/
setup_code:
	mov	ax,CON(SYSSEG)	
	mov	es,ax
	SEG_ES
	cmp	WLOC(0),CON(0xAA55)
	jne	badsig
	
/* If there's anything else to do, do it now. */

/* Jump to the bootrom code */
	jmp	SYSSEG:0x0006

badsig:	
	/* Put the stack at BOOTSEG:0x4000. */
	mov	cx,CON(BOOTSEG)
	mov	bx,CON(0xfff0)
	cli
	mov	ss,cx
	mov	sp,bx
	sti
	mov	dx,CON(SETUPSEG)
	mov	ds,dx
	mov	cx,CON(badsigend-badsigmsg)
	mov	si,CON(badsigmsg-setup_code)
	mov	bx,CON(0x0007)		/* page 0, attribute 7 (normal) */
	mov	ah,CON(0x0e)		/* write char, tty mode */
prloop2:
	lodsb
	int	0x10
	loop	prloop2
freeze2:
	JMP	freeze2
badsigmsg:
	STRDECL('No ROM signature')
	BYTE(13)
	BYTE(10)
badsigend:


	ORG(((SETUPSECS+1)*512)-1)
	BYTE(0)
/*
	That's about it.
*/
