; first.S  -  primary boot loader
;
; Copyright (C) 1995,1998 Gero Kuhlmann   <gero@gkminix.han.de>
; Copyright (C) 1996,1997 Gero Kuhlmann   <gero@gkminix.han.de>
;                and Markus Gutschke <gutschk@math.uni-muenster.de>
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation; either version 2 of the License, or
;  any later version.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this program; if not, write to the Free Software
;  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#if	!defined(USE_NASM) && !defined(USE_AS86)
#define	USE_AS86
#endif

#ifdef	USE_AS86
#define	CON(x)		*x
#define	BCON(x)		*x
#define	WCON(x)		*x
#define	LOC(x)		x
#define	BLOC(x)		byte ptr x
#define	WLOC(x)		word ptr x
#define	JMP(x)		jmp	x
#define	JMPN(x)		jmp	near x
#define	STRDECL(s)	.ascii	s
#define	SEGCS		seg	cs
#define	SEGDS		seg	ds
#define	SEGES		seg	es
#define	ALIGN(x)	.align	x
#define	SPACE(x)	.space	x
#endif

#ifdef	USE_NASM
#define	CON(x)		x
#define	BCON(x)		byte x
#define	WCON(x)		word x
#define	LOC(x)		[x]
#define	BLOC(x)		byte [x]
#define	WLOC(x)		word [x]
#define	JMP(x)		jmp	short x
#define	JMPN(x)		jmp	x
#define	STRDECL(s)	db	s
#define	SEGCS		cs
#define	SEGDS		ds
#define	SEGES		es
#define	ALIGN(x)	align x, db 0
#define	SPACE(x)	times x db 0
#endif

#ifndef ASM_DEBUG
#undef ASM_DEBUG
#endif
#include "first-linux.h"
#include "version-linux.h"

; Note on the following code: this has been written to run in real-86-mode.
; The type of processor check is made lateron in the Linux kernel, so let
; this code even work on a 8086 without a crash. The kernel will tell the
; user if a wrong processor is being used. Space and time are not quite
; crucial in this loader for not using raw-86-code.

#ifdef	USE_AS86
	.text
	.org	0
#endif
#ifdef	USE_NASM
	text
#endif

	mov	dx,ds
	mov	ax,cs			; set DS and ES
	mov	ds,ax
	mov	LOC(oldES),es
	mov	LOC(oldDS),dx		; save old register values in case
	mov	LOC(oldBP),bp		; we have to return to the boot rom
	mov	LOC(oldSI),si
	mov	LOC(oldDI),di
	mov	bp,sp
	mov	ax,[bp+4]
	mov	LOC(header+0),ax	; load the address of the boot header
	mov	ax,[bp+6]
	mov	LOC(header+2),ax
	mov	ax,[bp+8]
	mov	LOC(bootp+0),ax		; load the address of the bootp block
	mov	ax,[bp+10]
	mov	LOC(bootp+2),ax

; Tell the user who we are and that we started running

	mov	si,CON(sigmsg)
	call	prnstr

; Show program parameter addresses for debugging

#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	mov	si,CON(ad1msg)
	call	prnstr
	mov	ax,LOC(header+2)
	call	prnwrd
	mov	al,CON(0x3A)
	call	prnchr
	mov	ax,LOC(header+0)	; print header address
	call	prnwrd
	mov	si,CON(crlf)
	call	prnstr

	mov	si,CON(ad2msg)
	call	prnstr
	mov	ax,LOC(bootp+2)
	call	prnwrd
	mov	al,CON(0x3A)
	call	prnchr
	mov	ax,LOC(bootp+0)		; print BOOTP address
	call	prnwrd
	mov	si,CON(crlf)
	call	prnstr
#endif

; Check if the boot image header is correct.

	les	bx,LOC(header)
	mov	si,CON(bmerr)		; prepare for correct error message
	SEGES
	mov	ax,[bx+BOOT_HD_MAGIC+0]
	cmp	ax,LOC(bmagic+0)	; compare boot rom magic numbers
	jne	doerr1
	SEGES
	mov	ax,[bx+BOOT_HD_MAGIC+2]
	cmp	ax,LOC(bmagic+2)
	jne	doerr1

	mov	si,CON(vmerr)		; prepare for correct error message
	SEGES
	mov	al,[bx+BOOT_HD_LENGTH]
	mov	cl,CON(4)
	shr	al,cl
	and	al,CON(0x0F)
	cmp	al,CON(VENDOR_SIZE)	; check vendor ID size
	jne	doerr1
	xor	di,di
dovmag:	mov	al,[di+vmagic]		; check vendor ID
	or	al,al
	jz	chkmem			; vendor ID ok, continue
	SEGES
	cmp	al,[bx+di+BOOT_HD_VENDOR]
	jne	doerr1
	inc	di
	JMP(dovmag)
doerr1:	JMPN(doerr)

; Get the size of the base memory from the BIOS. We need at least
; 616 kB, which is the amount of base RAM below the boot rom memory
; area starting at 0x98000, plus the amount of static RAM needed for
; stack and command line. Note that this code just checks for the
; absolut minimum which is required. The actual amount of memory
; needed is determined lateron after determining the real base of
; the static memory area.

chkmem:	int	0x12			; get memory size in kB
	cmp	ax,CON(MIN_MEM)
	mov	si,CON(memerr)		; check if enough
	jb	doerr1
	mov	LOC(avlmem),ax		; save it for later

; Get the size of extended memory from the BIOS. This is needed for computing
; the address of a loaded ramdisk image.

	mov	ah,CON(0x88)
	int	0x15
	mov	LOC(extmem),ax

; Looks good. Now find the lowest usable spot of memory and check
; if there is still enough to hold all necessary dynamic memory. This
; ensures that we dont overwrite anything valuable when saving the
; bootp record out of the boot rom memory area.

	mov	bx,CON(BOOT_LD_ILENGTH)	; find the highest used memory
	call	fndmem			; block
#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	push	ax
	mov	dx,ax
	mov	si,CON(dynmsg)
	call	prnstr
	mov	ax,dx
	call	prnwrd			; lets see what we got here
	mov	si,CON(crlf)
	call	prnstr
	pop	ax
#endif
	mov	dx,ax
	mov	ax,cs
	mov	si,CON(recerr)
	cmp	dx,ax			; oops, shouldnt be below us
	jbe	doerr1
	mov	bx,CON(MAX_MEM - (MAX_DYNAMIC_LEN)/16 - 1)
	mov	si,CON(memerr)
	cmp	dx,bx			; check if enough memory
	ja	doerr1

; We can now setup the pointers to all dynamic data structures. Note
; that the bootp memory block comes last so that we can use larger
; bootp records if required (and supported by the server), up until
; the beginning of the boot rom area (0x98000).

setptr:	mov	LOC(adrptr+2),dx	; first set all segments
	mov	LOC(pthptr+2),dx
	mov	LOC(botptr+2),dx
	mov	WLOC(adrptr),CON(0)	; then update the offsets
	mov	WLOC(pthptr),CON(0+MAX_ADDRS_LEN)
	mov	WLOC(botptr),CON(0+MAX_ADDRS_LEN+MAX_PATH_LEN)

; Determine the kernel entry address. This is used to actually call
; the kernel lateron.

	mov	si,CON(recerr)
	mov	al,CON(VENDOR_SETUP)
	call	fndldr			; find load record for kernel
	mov	ax,es			; setup module
	or	ax,di
	jz	doerr4
	SEGES
	mov	al,[di+BOOT_LD_FLAGS]	; get load record flags
	test	al,CON(BOOT_FLAG_B0 + BOOT_FLAG_B1)	; we are presently unable
	jnz	doerr4			; to handle relative addrs
	SEGES
	mov	ax,[di+BOOT_LD_ADDR+0]	; get base address of setup module
	SEGES
	mov	bx,[di+BOOT_LD_ADDR+2]
	mov	dx,ax
	and	dx,BCON(0x000F)		; determine offset into setup segment
	mov	cl,CON(4)
	shr	ax,cl
	test	bx,CON(0xFFF0)		; address must be reachable
	jnz	doerr4
	mov	cl,CON(12)
	shl	bx,cl
	or	ax,bx			; determine segment pointer
	mov	LOC(kernel+0),dx		; save pointer
	mov	LOC(kernel+2),ax
#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	push	ax
	mov	si,CON(kermsg)
	call	prnstr
	pop	ax
	call	prnwrd			; let the user know
	mov	al,CON(0x3A)
	call	prnchr
	mov	ax,dx
	call	prnwrd
	mov	si,CON(crlf)
	call	prnstr
#endif
	JMP(chkrd)
doerr4:	JMPN(doerr)

; Now take care of pre-loaded ramdisk images and kernel images that have been
; loaded into extended memory; these features require Linux V1.3.73 or above.

chkrd:	call	fndcode32		; Kernel entry point for 32bit code
	or	si,si
	jnz	doerr4
	call	fndrd			; Check for pre-loaded ramdisk image
	or	si,si
	jnz	doerr4
	call	initsetup		; Initialize kernel setup data
	or	si,si
	jnz	doerr4

; Before we can use the memory area of the boot rom we have to secure
; all interesting data, e.g. either the bootp record or the path name
; (whichever is given by the boot rom). First check to see if we got
; a bootp reply here, and copy it to the dynamic memory area.

	cld
	xor	dx,dx
	les	di,LOC(bootp)
	SEGES
	mov	al,[di+BOOTP_OP]	; op code must indicate reply
	cmp	al,CON(BOOTP_REPLY)
	jne	dobot2			; it isnt
	add	di,CON(BOOTP_VEND)
	mov	bx,di
	mov	si,CON(pmagic)		; compare vendor ID
dobot1:	mov	di,bx
	mov	cx,CON(BOOTP_MAGIC_LEN)
	repe
	cmpsb
	jz	dobot3			; vendor ID is valid
	add	si,BCON(BOOTP_MAGIC_LEN)
#ifdef	USE_AS86
	cmp	byte ptr [si],BCON(0)	; check next vendor ID
#endif
#ifdef	USE_NASM
	cmp	[si],BCON(0)		; check next vendor ID
#endif
	jne	dobot1
dobot2:
#if defined(ASM_DEBUG)
	mov	si,CON(nobmsg)
	call	prnstr
#endif
	JMP(nobot2)			; vendor ID not found

dobot3:	sub	si,BCON(BOOTP_MAGIC_LEN)
	sub	si,CON(pmagic)
	mov	LOC(botid),si		; save vendor ID offset
	mov	ax,si
	push	ds
	les	di,LOC(botptr)
	lds	si,LOC(bootp)
	mov	bx,si
	mov	dx,CON(BOOTP_SIZE)
	or	ax,ax			; if not RFC vendor ID the bootp
	jnz	dobot7			; record has fixed length

	xor	cx,cx
	add	si,CON(BOOTP_VEND + BOOTP_MAGIC_LEN)
dobot4:	lodsb
	cmp	al,CON(BOOTP_RFC_NOP)	; handle NOP tag
	jnz	dobot5
	inc	cx
	cmp	cx,BCON(16)		; more than 16 NOP tags is VERY unusual
	jb	dobot4			; so the bootp record maybe broken
	mov	si,CON(btperr)
doerr6:	pop	ds
	JMPN(doerr)

dobot5:	cmp	al,CON(BOOTP_RFC_END)	; handle END tag
	jnz	dobot6
	mov	dx,si
	sub	dx,bx			; compute length of bootp record
	cmp	dx,CON(BOOTP_SIZE)
	jae	dobot7
	mov	dx,CON(BOOTP_SIZE)	; use minimum size
	JMP(dobot7)

nobot2:	JMP(nobotp)

dobot6:	lodsb				; handle all other tags
	mov	cl,al
	xor	ch,ch
	add	si,cx			; jump to next tag
	xor	cx,cx			; reset NOP counter
	JMP(dobot4)			; proceed with next tag

dobot7:	mov	si,CON(btperr)		; determine amount of dynamic memory
	mov	ax,CON(MAX_MEM)		; available for bootp record
	mov	cx,es			; (botptr+2)
	sub	ax,cx
	test	ax,CON(0xF000)
	jnz	dobot8
	mov	cl,CON(4)
	shl	ax,cl
	mov	cx,di			; (botptr+0)
	sub	ax,cx
	cmp	dx,ax			; check if bootp record fits into
	jae	doerr6			; dynamic memory area

dobot8:	mov	cx,dx
	mov	si,bx			; restore source pointer
	rep
	movsb				; save the bootp record
	pop	ds

#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	mov	si,CON(btpmsg)
	call	prnstr
	mov	ax,LOC(botid)
	call	prnwrd			; let the user know
	mov	si,CON(crlf)
	call	prnstr
#endif

nobotp:	mov	LOC(botlen),dx		; set length of bootp record
	or	dx,dx
	jnz	setmem			; check path name if bootp length is 0

; Some older versions of the boot rom code dont give the address of the
; bootp record but the address of the name of the boot image file on the
; server. Since we now know that we didnt get a valid bootp record, try
; the path name version next. Unfortunately there is no magic cookie
; available for checking the validity of the string (much older versions
; of the boot rom code didnt even provide this path name information;).
; The strlen routine will take care of checking the string: there shouldnt
; be any non-printable characters before the terminating 0, and the string
; should not be longer than allowed.

	xor	dx,dx
	push	ds
	lds	si,LOC(bootp)
	call	strlen			; get total string length
	pop	ds
	jcxz	nopath			; abort if string is invalid
	cmp	cx,CON(MAX_PATH_LEN)
	ja	nopath			; string should not be too long

#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	mov	si,CON(pthmsg)
	call	prnstr
#endif
	push	ds
	les	di,LOC(pthptr)
	lds	si,LOC(bootp)		; save the path name string
	mov	dx,cx
	rep
	movsb
	pop	ds
nopath:	mov	LOC(pthlen),dx		; set length of path name string

; At this point all dynamic data, which is only necessary for this boot
; loader, has been saved. Now we can go ahead and acquire memory for every-
; thing which must remain intact when we call the kernel. This is basically
; the stack and the command line. We can now use the boot rom data area
; for this, so first find the highest used reserved memory segment from
; all load records, and setup the stack and command line.

setmem:	mov	bx,CON(BOOT_LD_MLENGTH)	; find the highest reserved memory
	call	fndmem			; block
	cmp	ax,CON(MAX_MEM)		; should be at least in the boot rom
	jae	setm1			; data area
	mov	ax,CON(MAX_MEM)
setm1:	mov	dx,LOC(botptr+0)	; the static data should not interfere
	add	dx,LOC(botlen)		; with the dynamic data area. botptr
	add	dx,BCON(0x000F)		; is the last data field in the dynamic
	mov	cl,CON(4)		; area, so compute its ending segment
	shr	dx,cl			; and use that as the static base.
	add	dx,LOC(botptr+2)
	inc	dx
	cmp	ax,dx			; set end of dynamic data area as base
	jae	setm3			; of static area in case of conflict
	mov	ax,dx
setm3:
#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	push	ax
	mov	dx,ax
	mov	si,CON(stamsg)
	call	prnstr
	mov	ax,dx
	call	prnwrd			; lets see what we got here
	mov	si,CON(crlf)
	call	prnstr
	pop	ax
#endif

	mov	si,CON(recerr)
	mov	bx,ax			; BX - base of free memory area
	mov	al,CON(VENDOR_CMDL)
	call	fndldr			; find load record for cmd line
	mov	ax,es
	or	ax,di			; unable to find record
	jz	doerr
	SEGES
	mov	ax,[di+BOOT_LD_MLENGTH+0]	; get memory length of cmd line
	SEGES
	mov	dx,[di+BOOT_LD_MLENGTH+2]
	or	dx,dx			; command line is not allowed to be
	jnz	doerr			; longer than 4 kB
	cmp	ax,CON(CMDL_SIZE)
	ja	doerr
	mov	LOC(cmdmax),ax		; save maximum size of command line
	add	ax,CON(STACK_SIZE)	; compute size of static memory
	jc	doerr

	mov	si,CON(memerr)
	add	ax,CON(0x000F)		; round up to nearest segment
	mov	cl,CON(4)
	shr	ax,cl			; convert it into segment number
	mov	dx,ax			; save size for later use
	add	ax,bx			; add it to base segment
	add	ax,CON(0x003F)		; round to nearest kB
	mov	cl,CON(6)
	shr	ax,cl			; convert it into kB
	cmp	ax,LOC(avlmem)
	jb	setcmd			; we have enough memory

doerr:	call	prnstr			; in case of error return to the
	mov	si,LOC(oldSI)		; boot rom with registers set
	mov	di,LOC(oldDI)		; correctly
	mov	bp,LOC(oldBP)
	mov	es,LOC(oldES)
	mov	ds,LOC(oldDS)
	retf

; We can now set the command line pointer. This requires some conversion
; because the kernel expects the command line to be relative to the floppy
; boot loader segment.
; BX contains the first segment usable for static memory, and DX contains
; the required size of static memory in segments.

setcmd:	mov	si,CON(recerr)
	mov	al,CON(VENDOR_INIT)
	call	fndldr			; find load record for the INIT segment
	mov	ax,es
	or	ax,di			; isnt there?
	jz	doerr
	mov	cx,dx
	SEGES
	mov	al,[di+BOOT_LD_FLAGS]	; get load record flags
	test	al,CON(BOOT_FLAG_B0 + BOOT_FLAG_B1)	; we are presently unable
	jnz	doerr			; to handle relative addrs
	SEGES
	mov	dx,[di+BOOT_LD_ADDR+0]	; get base address of INIT segment
	SEGES
	mov	ax,[di+BOOT_LD_ADDR+2]
	test	dx,CON(0x000F)		; must be on a segment boundary
	jnz	doerr
	test	ax,CON(0xFFF0)		; must in in lower MB
	jnz	doerr
	push	cx
	mov	cl,CON(4)		; convert address into segment
	shr	dx,cl
	mov	cl,CON(12)
	shl	ax,cl			;    BX - first usable memory segment
	or	dx,ax			;    DX - segment of floppy boot loader
	pop	cx			;    CX - size of static memory in segs
	sub	bx,dx			; compute offset to first usable memory
	jb	doerr

	mov 	si,CON(memerr)
	mov	ax,bx			; the complete segment should not exceed
	add	ax,cx			; 64 kB
	jc	doerr
	test	ax,CON(0xF000)
	jnz	doerr
	mov	cl,CON(4)
	shl	bx,cl			; convert segment offset into byte offset
	add	bx,CON(STACK_SIZE)
	mov	LOC(cmdptr+0),bx	; save the command line pointer
	mov	LOC(cmdptr+2),dx

#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	mov	si,CON(cmdmsg)
	call	prnstr
	mov	ax,dx
	call	prnwrd
	mov	al,CON(0x3A)
	call	prnchr			; show the new command line pointer
	mov	ax,bx			; on the console
	call	prnwrd
	mov	si,CON(crlf)
	call	prnstr
#endif

; Now set the stack. From this point on there is no return to the boot rom.

	cli				; disable interrupts during stack setup
	dec	bx
	mov	sp,bx			; set the stack
	mov	ss,dx
	sti				; it is safe again

; At this point the memory layout should look like this, assuming default
; values for all load records set by mknbi:
;
; 0x10000-0x8FFFF	512.0 kB	kernel
; 0x90000-0x901FF	  0.5 kB	linux floppy boot sector
; 0x90200-0x911FF	  8.0 kB	first sectors of kernel setup
; 0x92200-0x931FF	  4.0 kB	primary boot loader - this program
; 0x93200-0x933FF	  0.5 kB	boot image header
; 0x93400-0x935FF	  0.5 kB	default command line
; 0x93600-0x937FF	  0.5 kB	dynamic memory - IP address string
; 0x93800-0x93BFF	  1.0 kB	dynamic memory - path name
; 0x93C00-0x93DFF	  0.5 kB	dynamic memory - bootp record
; 0x93E00-0x99FFF	 24.5 kB	(free - used lateron by kernel setup)
; 0x9A000-0x9AFFF	  4.0 kB	static memory - stack
; 0x9B000-0x9B7FF	  2.0 kB	static memory - kernel command line
; 0x9B800-0x9FFFF	 18.0 kB	(free)
;
; The remaining part of this program just deals with setting up the command
; line in the static memory area. First copy it to its rightful place and
; setup the pointers so that the kernel will find the new command line.

	push	ds
	push	bp
	xor	dx,dx			; DX contains length of command line
	mov	bp,LOC(cmdmax)		; BP contains maximum length of cmd line
	sub	bp,BCON(2)		; keep some space for trailing 0
	mov	al,CON(VENDOR_CMDL)
	call	fndldr			; find load record for cmd line
	mov	ax,es
	mov	si,di
	les	di,LOC(cmdptr)		; load desitination pointer
	mov	ds,ax
	or	ax,si			; unable to find command line
	jz	nocmd1			; should never happen
	mov	al,[si+BOOT_LD_FLAGS]	; get load record flags
	test	al,CON(BOOT_FLAG_B0 + BOOT_FLAG_B1)	; we are presently unable
	jnz	nocmd1			; to handle relative addrs

	mov	ax,[si+BOOT_LD_ADDR+0]	; load pointer to original command
	mov	bx,[si+BOOT_LD_ADDR+2]	; line and convert it into seg:ofs
	push	ax
	and	ax,CON(0x000F)		; compute offset
	mov	si,ax
	pop	ax
	mov	cl,CON(4)
	shr	ax,cl			; compute segment
	test	bx,CON(0xFFF0)
	jnz	nocmd1			; segment is unreachable in real mode
	mov	cl,CON(12)
	shl	bx,cl
	or	ax,bx
	mov	ds,ax			; DS:SI points to old command line

	call	strlen			; get length of string
	jcxz	nocmd1
	cmp	cx,bp			; check length of command line
	jbe	stcmd1
	mov	cx,bp
stcmd1:	mov	dx,cx			; save length for later
	cld
	rep
	movsb				; move it
nocmd1:	xor	al,al
	stosb				; terminate the string with 0
	pop	bp
	pop	ds

	mov	al,CON(VENDOR_INIT)
	call	fndldr			; find load record for floppy boot prg
	mov	ax,es
	or	ax,di			; oops, not there?
	jz	nocmd2			; should never happen
	SEGES
	mov	al,[di+BOOT_LD_FLAGS]	; get load record flags
	test	al,CON(BOOT_FLAG_B0 + BOOT_FLAG_B1)	; we are presently unable
	jnz	nocmd2			; to handle relative addrs

	SEGES
	mov	ax,[di+BOOT_LD_ADDR+0]	; load pointer to floppy boot sector
	SEGES
	mov	bx,[di+BOOT_LD_ADDR+2]
	push	ax
	and	ax,CON(0x000F)		; compute offset
	mov	di,ax
	pop	ax
	mov	cl,CON(4)
	shr	ax,cl			; compute segment
	test	bx,CON(0xFFF0)
	jnz	nocmd2			; segment is unreachable in real mode
	mov	cl,CON(12)
	shl	bx,cl
	or	ax,bx
	mov	es,ax			; ES:DI points to floppy boot sector
	mov	LOC(flopptr+0),di
	mov	LOC(flopptr+2),es

	mov	bx,LOC(cmdptr+0)
	sub	bx,di
	mov	cx,LOC(cmdptr+2)	; compute offset to command line
	sub	cx,ax			; from base of init segment
	jc	nocmd2
	test	cx,CON(0xF000)
	jnz	nocmd2			; offset too large
	mov	ax,cx
	mov	cl,CON(4)
	shl	ax,cl			; convert seg:ofs representation into
	add	ax,bx			; absolute number of bytes

	SEGES				; setup command line descriptor
	mov	[di+CL_MAGIC_ADDR],WCON(CL_MAGIC)
	SEGES
	mov	[di+CL_OFFSET],ax
	JMP(stcmd2)
nocmd2:	xor	dx,dx			; error with command line
stcmd2:	mov	LOC(cmdlen),dx		; save command line length

#ifdef ASM_DEBUG_VERBOSE
	mov	ax,dx
	call	prnwrd
	mov	al,CON(0x20)
	call	prnchr
	push	ds
	lds	si,LOC(cmdptr)		; print the command line so far
	call	prnstr
	pop	ds
	mov	si,CON(crlf)
	call	prnstr
#endif

#ifdef	FALLBACK_TO_TFTPDIR
; Now determine the name of the root directory on the server by looking
; into the bootp record. If there is no bootp record, the path has probably
; already been set directly from a parameter given by the boot rom. If
; a boot image file name was found, only leave the directory name by
; removing everything after the last slash. There is also the possibility
; to define a root directory name in the vendor area of the bootp record,
; so check for that also.

	cmp	LOC(botlen),WCON(BOOTP_SIZE)
	jb	bootp3			; bootp record not loaded
	push	ds
	mov	bx,LOC(pthlen)		; keep old path length for reference
	mov	dx,CON(MAX_PATH_LEN)
	sub	dx,BCON(2)		; keep space in path name for trailing 0
	les	di,LOC(pthptr)
	lds	si,LOC(botptr)
	add	si,BCON(BOOTP_FILE)
	call	strlen			; determine length of file name
	jcxz	bootp2			; no file name, leave path untouched
	cmp	cx,dx
	jbe	bootp1
	mov	cx,dx			; get length of path name
bootp1:	mov	bx,cx			; save length  for later use
	cld
	rep
	movsb				; copy path name to its new location
	xor	al,al
	stosb				; save trailling 0
bootp2:	pop	ds
	mov	LOC(pthlen),bx		; save new length of path name

bootp3:	xor	dx,dx
	mov	cx,LOC(pthlen)		; No file name given
	jcxz	bootp4
	std
	les	di,LOC(pthptr)
	add	di,cx			; point ES:DI to end of string
	mov	al,CON(0x2F)		; search backwards for last occurrence
	repne				; of a slash
	scasb
	jnz	bootp4			; not found -> no path
	SEGES
#ifdef	USE_AS86
	mov	byte ptr [di+1],BCON(0)	; replace the slash with 0
#endif
#ifdef	USE_NASM
	mov	BLOC(di+1),BCON(0)	; replace the slash with 0
#endif
	mov	dx,cx
	inc	dx
bootp4:	mov	LOC(pthlen),dx		; store length of directory name
#endif

	mov	al,CON(BOOTP_RFC_ROOT)	; there is still another possibility:
	call	gettag			; get the directory name from vendor
	jcxz	bootp5			; field of bootp record
	mov	dx,cx
	push	ds
	cld
	mov	ax,es
	mov	si,di
	les	di,LOC(pthptr)		; copy the directory name from the
	mov	ds,ax			; bootp record
	rep
	movsb
	xor	al,al
	stosb				; terminate string with 0
	pop	ds
	mov	LOC(pthlen),dx

bootp5:
#ifdef ASM_DEBUG_VERBOSE
	mov	ax,LOC(pthlen)
	call	prnwrd
	mov	al,CON(0x20)
	call	prnchr
	push	ds
	lds	si,LOC(pthptr)		; print out the path
	call	prnstr
	pop	ds
	mov	si,CON(crlf)
	call	prnstr
#endif

; Build the IP address string by looking into the bootp record. If there
; is no bootp record available, just dont do anything, and leave the
; string empty.

	xor	dx,dx
	les	di,LOC(adrptr)
	cmp	LOC(botlen),WCON(BOOTP_SIZE)
	jae	doaddr
	JMPN(noaddr)			; no bootp record, no address string

doaddr:	cld
	push	ds
	lds	si,LOC(botptr)
	add	si,BCON(BOOTP_YIADDR)	; load my own IP address
	call	cvtadr			; put address into destination string
	mov	al,CON(0x3A)		; add ':'
	call	cvtchr
	add	si,BCON(BOOTP_SIADDR - BOOTP_YIADDR)
	call	cvtadr			; put server address into dest string
	mov	al,CON(0x3A)		; add ':'
	call	cvtchr
	pop	ds

	push	es
	push	di
	mov	al,CON(BOOTP_RFC_GWY)	; we have to take the gateway address
	call	gettag			; from the vendor info area
	mov	ax,es
	mov	si,di
	pop	di
	pop	es
	jcxz	noadd1			; no netmask found
	push	ds
	mov	ds,ax
	call	cvtadr			; put gateway address into dest string
	pop	ds
noadd1:	mov	al,CON(0x3A)		; add ':'
	call	cvtchr

	push	es
	push	di
	mov	al,CON(BOOTP_RFC_MSK)	; we have to take the netmask from the
	call	gettag			; vendor info area
	mov	ax,es
	mov	si,di
	pop	di
	pop	es
	jcxz	noadd2			; no netmask found
	push	ds
	mov	ds,ax
	call	cvtadr			; put netmask address into dest string
	pop	ds
noadd2:	mov	al,CON(0x3A)		; add ':'
	call	cvtchr

	push	es
	push	di
	mov	al,CON(BOOTP_RFC_HNAME)	; get the host name from the vendor
	call	gettag			; info area
	mov	ax,es
	mov	si,di
	pop	di
	pop	es
	jcxz	noadd3			; no host name found
	push	ds
	mov	ds,ax
	add	dx,cx			; update string length
	rep
	movsb				; move host name into dest string
	pop	ds

noadd3:	push	es
	push	di
	mov	al,CON(BOOTP_RFC_RDEV)	; get the name for the network device
	call	gettag			; by which to reach the root FS
	mov	ax,es
	mov	si,di
	pop	di
	pop	es
	jcxz	noaddr			; no device name found
	push	ds
	mov	ds,ax
	mov	al,CON(0x3A)		; only add ':' if there is a device name
	call	cvtchr
	add	dx,cx			; update string length
	rep
	movsb				; move device name into dest string
	pop	ds

noaddr:	xor	al,al			; terminate string with 0
	stosb
	mov	LOC(adrlen),dx		; save string length

#ifdef ASM_DEBUG_VERBOSE
	mov	ax,dx
	call	prnwrd
	mov	al,CON(0x20)
	call	prnchr
	push	ds
	lds	si,LOC(adrptr)		; let the user know
	call	prnstr
	pop	ds
	mov	si,CON(crlf)
	call	prnstr
#endif

; Now we can create the final command line which can be passed to the
; kernel. This includes adding any additional arguments from the BOOTP
; record.

	mov	si,CON(rtstr)
	mov	cx,CON(rtend - rtstr)
	les	di,LOC(pthptr)
	mov	ax,LOC(pthlen)		; first handle root directory name
	or	ax,ax
	jnz	setup1
	mov	es,ax
	mov	di,ax
setup1:	call	procv			; setup command line

	mov	si,CON(adrstr)
	mov	cx,CON(adrend - adrstr)
	les	di,LOC(adrptr)
	mov	ax,LOC(adrlen)		; then handle the IP address string
	or	ax,ax
	jnz	setup2
	mov	es,ax
	mov	di,ax
setup2:	call	procv			; setup command line
	call	vparms			; process kernel parameters in vendor
					; extension tags
	mov	al,CON(BOOTP_RFC_CMDAD)	; check for application vendor tag
	call	gettag
	jcxz	setup			; tag not available
	mov	si,CON(nulstr)
	call	addarg			; add string to end of command line

; Finally call the setup code of the kernel

setup:
	call	vga			; process "vga=" argument

#ifdef ASM_DEBUG_VERBOSE
	mov	ax,LOC(cmdlen)
	call	prnwrd
	mov	al,CON(0x20)
	call	prnchr
	push	ds
	lds	si,LOC(cmdptr)		; Print the new command line for
	call	prnstr			; debugging
	pop	ds
#endif
#ifdef ASM_FREEZE_AFTER_INIT
lop1:	JMP(lop1)			; This is a feature, not a bug...
#endif
#ifdef ASM_KEYPRESS_AFTER_INIT
	xor	ah,ah
	int	0x16
#endif
	jmp	far [kernel]		; call the kernel



;====================================================================
;
; Convert an IP address pointed to by DS:SI into a string pointed
; to by ES:DI. The length of the string gets accumulated in DX.
; ES:DI points to first character after string.
;
; Changed registers: AX, DX, DI, ES

cvtadr:	push	bx
	mov	ax,[si]
	or	ax,[si+2]		; if address is 0.0.0.0, dont
	jz	cvtad9			; convert it

	xor	bx,bx
cvtad1:	mov	al,[si+bx]
	call	cvtdec			; convert each byte in turn into a
	cmp	bx,BCON(3)		; decimal number
	jae	cvtad9			; abort if at last byte of address
	mov	al,CON(0x2E)		; add '.'
	call	cvtchr
	inc	bx			; proceed with next byte
	JMP(cvtad1)

cvtad9:	pop	bx
	ret



;====================================================================
;
; Convert byte in AL into a decimal number string pointed to by
; ES:DI. The length of the string gets accumulated in DX. Return
; a pointer to first character after string in ES:DI.
;
; Changed registers: AX, DX, DI, ES

cvtdec:	push	cx
	xor	ah,ah
	xor	ch,ch
	mov	cl,CON(100)
	div	cl			; get 100th
	or	ch,al			; dont print if zero
	jz	cvtd1
	call	cvtd7			; convert digit into ascii character
cvtd1:	mov	al,ah
	xor	ah,ah
	mov	cl,CON(10)
	div	cl			; get 10th
	or	ch,al			; dont print if zero
	jz	cvtd2
	call	cvtd7			; convert digit into ascii character
cvtd2:	mov	al,ah			; get 1th
	pop	cx
cvtd7:	add	al,CON(0x30)		; convert digit into ascii character
cvtchr:	cmp	dx,CON(MAX_ADDRS_LEN)
	jae	cvtch9
	stosb
	inc	dx
cvtch9:	ret



;====================================================================
;
; Scan all load records, and find the highest used memory address,
; round it up to the nearest segment boundary and return it in AX.
; The offset to the length value in the load records is given in BX.
; Ignore all memory segments that have been loaded into extended memory;
;
; Changed registers: AX, DI, ES

fndmem:	push	cx
	push	dx
	les	di,LOC(header)
	mov	ax,di
	add	ax,CON(0x000F)
	and	ax,CON(0xFFF0)		; round up to nearest segment
	mov	cl,CON(4)
	shr	ax,cl
	mov	cx,es			; convert address of header into
	add	ax,cx			; segment representation, and add
	jc	fndm7			; the size of the header in order to
	add	ax,CON(BOOT_SIZE / 16)	; get a starting value for the highest
	jnc	fndm8			; memory segment.
fndm7:	xor	ax,ax			; the highest segment is on the stack
fndm8:	push	ax

	SEGES
	mov	al,[di+BOOT_HD_LENGTH]	; get length of image header
	call	getlen
	add	di,ax			; get the pointer to first load record
fndm1:	SEGES
	mov	al,[di+BOOT_LD_FLAGS]	; get load record flags
	test	al,CON(BOOT_FLAG_B0 + BOOT_FLAG_B1)	; we are presently unable
	jnz	fndm4				; to handle relative addrs
	SEGES
	mov	dx,[di+BOOT_LD_ADDR+0]
	SEGES
	mov	ax,[di+BOOT_LD_ADDR+2]	; get the load address
	SEGES
	add	dx,[di+bx+0]		; add the length (offset in BX)
	SEGES
	adc	ax,[di+bx+2]
	add	dx,BCON(0x000F)		; round up to the nearest segment
	adc	ax,CON(0)
	and	dx,CON(0xFFF0)
	test	ax,CON(0xFFF0)		; only allow addresses in lower 1 MB
	jnz	fndm4

	mov	cl,CON(4)			; convert address into segment address
	shr	dx,cl			; by shifting everything right by 4 bits
	mov	cl,CON(12)
	shl	ax,cl
	or	ax,dx			; combine higher and lower word of addr
	pop	dx
	cmp	ax,dx			; is new address higher than old one?
	jbe	fndm3
	mov	dx,ax			; yes, save it
fndm3:	push	dx

fndm4:	SEGES
	mov	al,[di+BOOT_LD_FLAGS]	; ok, thats it, now check if thats the
	test	al,CON(BOOT_FLAG_EOF)	; last record
	jnz	fndm5
	SEGES
	mov 	al,[di+BOOT_LD_LENGTH]	; no, get the address of the next one
	call	getlen
	add	di,ax
	JMP(fndm1)
fndm5:	pop	ax			; return the highest used segment
	pop	dx
	pop	cx
	ret



;====================================================================
;
; Find a load record in the boot header. The ID number of the load
; record is in AL, and ES:DI points to requested load record, or is
; the NULL pointer if load record not found.
;
; Changed registers: AX, DI, ES

fndldr:	push	cx
	mov	ch,al
	les	di,LOC(header)		; examine boot image header
	SEGES
	mov	al,[di+BOOT_HD_LENGTH]	; get length of image header
	call	getlen
	add	di,ax			; get the pointer to first load record
fndl1:	SEGES
	cmp	ch,[di+BOOT_LD_TAG1]	; is it the desired one ?
	je	fndl3
	SEGES
	mov	al,[di+BOOT_LD_FLAGS]	; no, so check if its the last record
	test	al,CON(BOOT_FLAG_EOF)
	jnz	fndl2
	SEGES
	mov 	al,[di+BOOT_LD_LENGTH]	; no, get the address of the next one
	call	getlen
	add	di,ax
	JMP(fndl1)

fndl2:	xor	ax,ax			; couldnt find the desired record
	mov	es,ax
	mov	di,ax
fndl3:	pop	cx
	ret



;====================================================================
;
; Compute the length of a load record address from a length byte
; in AL. Return the offset in AX.
;
; Changed registers: AX

getlen:	push	cx
	mov 	ah,al
	mov 	cl,CON(4)
	shr	ah,cl
	and	ax,CON(0x0f0f)		; compute the total length in
	add	al,ah			; bytes from the length of the
	xor	ah,ah			; record and that of the vendor
	shl	ax,1			; information.
	shl	ax,1
	pop	cx
	ret



;====================================================================
;
; Find a vendor tag in the bootp record. The tag number is in AL.
; It returns the pointer to the tag string in ES:DI and the length
; of the string in CX. CX is zero if tag not found.
;
; Changed registers: AX, CX, DI, ES

gettag:	cld
	push	si
	push	dx
	xor	dx,dx
	cmp	LOC(botlen),WCON(BOOTP_SIZE)
	jb	gett8			; no bootp record available
	cmp	WLOC(botid),BCON(0)	; use only RFC vendor information
	jne	gett8
	les	di,LOC(botptr)
	mov	si,di
	add	si,LOC(botlen)		; let ES:SI point to last bootp byte
	add	di,CON(BOOTP_VEND)
	add	di,BCON(BOOTP_MAGIC_LEN)

	mov	ah,al			; save tag number
	xor	ch,ch
gett1:	SEGES
	mov	al,[di]			; load current vendor tag
	inc	di
	cmp	al,CON(BOOTP_RFC_NOP)	; NOP tag --> continue
	je	gett1
	cmp	al,CON(BOOTP_RFC_END)	; END tag --> abort
	je	gett8
	SEGES
	mov	cl,[di]			; get length of current tag
	inc	di
	cmp	al,ah			; we got our tag
	je	gett9
	add	di,cx			; proceed with next tag
	cmp	di,si
	jae	gett8			; got at end of bootp record
	JMP(gett1)

gett8:	mov	cx,dx
gett9:	pop	dx
	pop	si
	ret



;====================================================================
;
; Process an argument on the command line. This routine handles
; basically three cases:
;   - the argument value is "rom", which means to use the value
;     delivered by the boot rom
;   - the argument value is "kernel", which means to just remove
;     the argument from the command line and then let the kernel
;     use its defaults
;   - the argument value contains neither "rom" nor "kernel", so
;     dont touch it.
;
; The name of the argument to look for in the command line is given
; in DS:SI with the its length in CX. The boot rom value is pointed
; to by ES:DI. If ES:DI is the NULL pointer, no boot rom value is known.
;
; Changed registers: AX, BX, CX, DX, SI, DI, ES

procv:	push	bp
	push	es
	push	di
	les	di,LOC(cmdptr)
	mov	dx,di			; load pointer to command line and
	call	strstr			; find option name
	jcxz	procv3			; option not found in cmd line
	mov	bx,di			;    ES:BX points to option name
	add	di,cx			;    ES:DI points to argument
	add	dx,LOC(cmdlen)		;    DX    contains remaining cmd line
	sub	dx,di
	jle	procv2			; length of remaining cmd line is zero
	mov	cx,dx
	mov	dx,di
	mov	al,CON(0x20)		; find end of argument (terminated by
	repne				; blank) or end of string (e.g. CX=0)
	scasb
	jcxz	procv1			; argument goes to end of string
	dec	di
procv1:	sub	di,dx
	xchg	dx,di			; compute length of argument in DX
	jg	procv4			; argument length is greater than zero

procv2:	mov	dx,si
	mov	di,bx			; if we got here, we found the option
	call	rmarg			; name but no argument, so just remove
	mov	si,dx			; the option from the cmd line.
procv3:	xor	dx,dx
procv4:	pop	bp
	pop	cx			; get pointer to boot rom value

; At this point the registers are allocated as follows:
;      DS:SI  points to name of option
;      CX:BP  points to boot rom value
;      ES:BX  points to option name in command line
;      ES:DI  points to argument in command line
;      DX     contains length of argument; if 0, BX and DI are invalid
;
; After all that we have a couple of choices for constructing the kernel
; command line:
;
; argument not given in command line:
;   -  if there is no value from the boot rom, just do nothing and let
;      the kernel decide
;   -  if there is a value from the boot rom, add a new argument at the
;      end of the command line using the boot rom value
;
; argument given in command line:
;   -  if the value of the argument is not empty and not "rom" or "kernel",
;      just do nothing
;   -  if the value of the argument is "rom", then remove the argument
;      from the command line and add a new one with the boot rom value, but
;      only if a boot rom value is available
;   -  if the value of the argument is "kernel", then remove the argument
;      from the command line and do nothing, thus letting the kernel decide

	or	dx,dx			; check if argument found in command
	jnz	isarg			; line

	push	ds
	mov	ax,cx
	mov	ds,ax
	or	ax,bp
	jz	procv5
	SEGDS				; if no boot rom value is available,
	mov	al,[bp]			; e.g. the string pointer is NULL or
	or	al,al			; the string length is zero, then
	jz	procv5			; just return and do nothing.
	pop	ds
	JMP(procv6)
procv5:	pop	ds
	JMP(procv9)

procv6:	mov	es,cx
	mov	di,bp			; if not, add new option with boot rom
	call	addarg			; argument to end of command line
procv9:	pop	bp
	ret

; Original command line contains the option we are looking for. Check whether
; the argument is one of the strings "rom" or "kernel"

isarg:	cld
	push	cx
	push	si
	mov	ax,di
	mov	cx,dx
	cmp	cx,CON(romend - romstr)
	jne	norom
	mov	si,CON(romstr)
	repe
	cmpsb				; compare for "rom"
	mov	di,ax
	jz	dorom
norom:	mov	cx,dx
	cmp	cx,CON(kerend - kerstr)
	jne	nokern
	mov	si,CON(kerstr)
	repe
	cmpsb				; compare for "kernel"
	mov	di,ax
	jz	dokern
nokern:	pop	si
	pop	cx			; not found --> do nothing
	JMP(procv9)

; the argument is "rom", so remove the existing argument from the
; command line, and add the boot rom path

dorom:	mov	di,bx
	call	rmarg			; remove old option containing
	pop	si			; "rom" argument
	pop	es
	mov	di,bp

	mov	ax,es
	or	ax,di
	jz	procv9
	SEGES				; if no boot rom value is available,
	mov	al,[di]			; e.g. the string pointer is NULL or
	or	al,al			; the string length is zero, then
	jz	procv9			; just return and do nothing.
	call	addarg			; add new option with boot rom value
	JMP(procv9)			; as argument

; the argument is "kernel", so just remove the existing argument from
; the command line and let the kernel decide by itself.

dokern:	mov	di,bx
	call	rmarg			; remove old option containing
	pop	si			; "kernel" argument
	pop	cx
	JMP(procv9)



;====================================================================
;
; Add a new option to the end of the command line. DS:SI points to
; name of option, and ES:DI points to argument value. Return ES:DI
; pointing to the trailing 0 of the command line, and the new length
; of the command line in AX.
;
; Changed registers: AX, SI, DI, ES

addarg:	push	cx
	push	dx
	call	strlen			; first compute the new length of
	mov	dx,cx			; the command line. if the new
	xchg	si,di			; argument will not fit into the
	push	ds			; memory provided, then just return
	mov	ax,es			; and leave the command line
	mov	ds,ax			; unchanged.
	call	strlen
	pop	ds
	xchg	si,di
	add	cx,dx
	pop	dx
	add	cx,LOC(cmdlen)
	inc	cx
	cmp	cx,LOC(cmdmax)
	pop	cx
	jae	addar9

	cld
	push	bx
	push	ds
	push	di
	push	es
	les	di,LOC(cmdptr)
	mov	bx,di
	add	di,LOC(cmdlen)
	mov	al,CON(0x20)		; add a blank
	stosb
addar1:	lodsb				; add the option name
	or	al,al
	jz	addar2
	stosb
	JMP(addar1)
addar2:	pop	ds
	pop	si			; load the argument pointer
addar3:	lodsb				; add the argument
	or	al,al
	jz	addar4
	stosb
	JMP(addar3)
addar4:	pop	ds			; restore DS and add trailing 0
	xor	al,al
	stosb
	dec	di
	mov	ax,di
	sub	ax,bx
	mov	LOC(cmdlen),ax		; adjust length of command line
	pop	bx
addar9:	ret



;====================================================================
;
; Remove an argument pointed to by ES:DI from the command line.
; Return ES:DI pointing to the trailing 0 of the command line, and
; the new length of the command line in AX.
;
; Changed registers: AX, SI, DI, ES

rmarg:	cld
	push	bx
	push	cx
	push	dx
	push	ds
	lds	si,LOC(cmdptr)
	mov	ax,ds			; get beginning of command line
	mov	es,ax
	mov	bx,si
	mov	si,di
	call	strlen			; get remaining length of cmd line
	mov	ax,CON(0x20)		; find end of argument in command line
	repne				; DS:SI points to first character after
	scasb				;       the argument string
	dec	di			; ES:DI points to start of argument
	xchg	si,di			; ES:BX points to start of cmd line
	or	cx,cx			; argument is not at the end of the
	jnz	rmarg2			; command line

rmarg1:	cmp	di,bx
	je	rmarg4			; empty command line
	dec	di			; remove leading blanks
	SEGES
#ifdef	USE_AS86
	cmp	byte ptr [di],BCON(0x20)
#endif
#ifdef	USE_NASM
	cmp	BLOC(di),BCON(0x20)
#endif
	je	rmarg1
	inc	di			; point to first blank after preceeding
	JMP(rmarg4)			; argument and make it end of string

rmarg2:	lodsb
	cmp	al,CON(0x20)
	je	rmarg2			; remove trailing blanks
rmarg3:	or	al,al
	jz	rmarg1			; end of command line... remove all
	stosb				; trailing blanks. Otherwise copy
	lodsb				; the remainder of the command line
	JMP(rmarg3)			; over the argument

rmarg4:	xor	al,al
	stosb				; add trailing 0
	pop	ds
	pop	dx
	pop	cx
	dec	di			; point to trailing 0
	mov	ax,di
	sub	ax,bx			; adjust length of command line
	mov	LOC(cmdlen),ax
	pop	bx
	ret



;====================================================================
;
; Find a substring (DS:SI, length CX) in a string (ES:DI). Return
; the substring position in ES:DI. CX is unchanged if substring found,
; otherwise CX is 0.
;
; Changed registers: AX, CX

strstr:	push	bx
	push	dx
	cld				; DX contains length of substring
	mov	dx,cx			; ES:BX points to string
	mov	bx,di			; DS:SI points to substring

strst1:	SEGES
	mov	al,[bx]
	or	al,al			; if at end of string we could not find
	jz	strst2			; the substring
	mov	di,bx
	mov	cx,dx			; reset substring
	mov	ax,si
	repe				; compare substring at current string
	cmpsb				; position
	mov	si,ax
	jz	strst3			; found it!
	inc	bx			; compare cmd line at next char
	JMP(strst1)

strst2:	xor	dx,dx			; could not find string
strst3:	mov	cx,dx			; set return values
	mov	di,bx
	pop	dx
	pop	bx
	ret



;====================================================================
;
; Determine length of string (DS:SI), and return it in CX
;
; Changed registers: AL, CX

strlen: push	si
	xor	cx,cx
	cld
strl1:	lodsb
	or	al,al
	jz	strl3
	cmp	al,CON(0x20)		; character should be printable
	jl	strl2
	cmp	al,CON(0x7E)
	ja	strl2
	inc	cx			; count character
	JMP(strl1)
strl2:	xor	cx,cx			; return 0 length if string invalid
strl3:	pop	si
	ret



;====================================================================
;
; Print a string in DS:SI onto the console
;
; Changed registers: AL

prnstr:	push	si
	cld
prns1:	lodsb				; loop over all characters of
	or	al,al			; string
	jz	prns2
	call	prnchr			; print character
	JMP(prns1)
prns2:	pop	si
	ret



;====================================================================
;
; Print hexadecimal values (in AX or AL) or characters onto the console
;
; Changed registers: AX

prnwrd:	push	ax
	mov	al,ah
	call	prnbyt			; print the upper byte
	pop	ax
prnbyt: push	ax
	shr	al,1			; prepare upper nibble
	shr	al,1
	shr	al,1
	shr	al,1
	call	prnnib			; print it
	pop	ax
prnnib:	and	al,CON(0x0F)		; prepare lower nibble
	add	al,CON(0x30)
	cmp	al,CON(0x39)		; convert it into hex
	jle	prnchr
	add	al,CON(7)
prnchr:	push	bx
	mov	ah,CON(0x0E)		; print it
	mov	bl,CON(0x07)
	xor	bh,bh
	int	0x10
	pop	bx
	ret



;====================================================================
;
; Check whether the kernel has been loaded high; set the load_flags
; accordingly and patch the kernel entry point in the setup code
; if neccessary. If an error occurred, SI contains a pointer to the
; error message.
;
; Changed registers: AX, BX, CX, DX, SI, ES, DI

fndcode32:
	mov	si,CON(recerr)
	mov	al,CON(VENDOR_KERNEL)
	call	fndldr			; find load record for kernel image
	mov	ax,es
	or	ax,di
	jz	f32ret
	SEGES
	mov	al,[di+BOOT_LD_FLAGS]	; get load record flags
	test	al,CON(BOOT_FLAG_B0 + BOOT_FLAG_B1)	; we are presently unable
	jnz	f32ret			; to handle relative addrs
	SEGES
	mov	ax,[di+BOOT_LD_ADDR+0]	; get base address of setup module
	SEGES
	mov	bx,[di+BOOT_LD_ADDR+2]
	mov	dx,CON(SU_MY_LOAD_FLAGS+1)	; "big" kernel
	test	bx,CON(0xFFF0)		; test if the kernel is loaded high
	jnz	bzimg
	mov	cl,CON(4)
	shr	ax,cl			; calculate start address in segment
	mov	dx,bx			; format
	shr	bx,cl
	mov	cl,CON(12)
	shl	dx,cl
	or	ax,dx
	mov	dx,CON(SU_MY_LOAD_FLAGS)	; "small" kernel
bzimg:	mov	LOC(lflags),dx		; update loadflags
	mov	LOC(code32+0),ax	; save pointer to 32bit kernel entry
	mov	LOC(code32+2),bx
#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	push	ax
	push	bx
	mov	si,CON(cd32msg)
	call	prnstr
	pop	ax
	call	prnwrd			; let the user know
	pop	ax
	call	prnwrd
	mov	si,CON(crlf)
	call	prnstr
#endif
	xor	si,si			; no error
f32ret:	ret



;====================================================================
;
; Check whether a ramdisk image is attached to the current netboot image;
; if a ramdisk is found, determine load address and size. If an error
; occurred, SI contains a pointer to the error message.
;
; Changed registers: AX, BX, CX, DX, SI, ES, DI

fndrd:	xor	si,si
	mov	al,CON(VENDOR_RAMDISK)
	call	fndldr			; find load record for ramdisk image
	mov	ax,es
	or	ax,di
	jz	rdret2			; no ramdisk image found
	mov	si,CON(recerr)
	SEGES
	mov	ax,[di+BOOT_LD_ILENGTH]	; determine size of ramdisk image
	mov	LOC(rdsize+0),ax
	SEGES
	mov	ax,[di+BOOT_LD_ILENGTH+2]
	mov	LOC(rdsize+2),ax
	SEGES
	mov	bl,[di+BOOT_LD_FLAGS]	; get load record flags
	test	bl,CON(BOOT_FLAG_B0)
	jnz	rdret2			; error: neither RD_FIX/AUTO nor RD_EOM
	mov	ax,LOC(extmem)		; determine end of physical memory
	test	bl,CON(BOOT_FLAG_B1)
	jnz	rdeom			; ramdisk is at end of memory
	SEGES			; check if ramdisk image needs to be
	mov	bl,[di+BOOT_LD_VENDOR]	; relocated to end of memory
	cmp	bl,CON(RD_AUTO)
	jne	rdfix			; ramdisk is at fixed location
	cmp	ax,CON(0x3c00)		; there is more then 16MB
	jbe	rdeom
	mov	ax,CON(0x3c00)		; limit to 16MB for BIOS
rdeom:	xor	bx,bx
	add	ax,CON(0x400)		; add size of conventional memory
	adc	bx,BCON(0)
	mov	dx,ax			; multiply by 1024
	mov	cl,CON(6)
	shr	dx,cl
	mov	cl,CON(10)
	shl	ax,cl
	shl	bx,cl
	or	bx,dx			; this is not strictly correct: for
	SEGES				; RD_EOM loading, we should get the
	sub	ax,[di+BOOT_LD_MLENGTH+0]; address rather than the memory size,
	SEGES				; but that would make the code for
	sbb	bx,[di+BOOT_LD_MLENGTH+2]; RD_AUTO loading far more difficult...
	SEGES				; check if ramdisk image needs to be
	mov	cl,[di+BOOT_LD_VENDOR]	; relocated to end of memory
	cmp	cl,CON(RD_AUTO)
	jne	rddone
	SEGES
	mov	cx,[di+BOOT_LD_ADDR+0]	; source address
	SEGES
	mov	dx,[di+BOOT_LD_ADDR+2]
	call	moverd			; move ramdisk
	or	si,si
	jz	rddone			; check if error
rdret2:	JMP(rdret)
rdfix:	SEGES
	mov	ax,[di+BOOT_LD_ADDR+0]
	SEGES
	mov	bx,[di+BOOT_LD_ADDR+2]
rddone:	mov	LOC(ramdisk+0),ax
	mov	LOC(ramdisk+2),bx
#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	push	ax
	push	bx
	mov	si,CON(rdmsg)
	call	prnstr
	pop	ax
	call	prnwrd			; let the user know
	pop	ax
	call	prnwrd
	mov	al,CON(0x28)
	call	prnchr
	mov	ax,LOC(rdsize+2)
	call	prnwrd
	mov	ax,LOC(rdsize+0)
	call	prnwrd
	mov	ax,CON(0x29)
	call	prnchr
	mov	si,CON(crlf)
	call	prnstr
#endif
	xor	si,si
rdret:	ret



;====================================================================
;
; Move the ramdisk image from the location given in DX/CX to BX/AX. If
; an error occurred, SI contains a pointer to the error message.
;
; Changed registers: CX, DX, SI

moverd:	push	ax			; save all registers that might
	push	bx			; be mangled
	push	es
	push	di
	mov	LOC(rd_dstb),ax		; enter destination address into gdt
	mov	LOC(rd_srcb),cx		; enter source address into gdt
	mov	cx,LOC(rdsize+0)	; number of bytes that need to be moved
	mov	ax,LOC(rdsize+2)
	add	cx,BCON(1)		; round up to next word
	adc	ax,CON(0)
	add	bl,al			; move from the top downwards
	add	dl,al
	mov	LOC(rd_dstb+2),bl
	mov	LOC(rd_srcb+2),dl
	shr	cx,CON(1)		; INT15 moves words
	jz	mvzero
	push	ax			; save register
	push	cs			; pass gdt in es:si
	pop	es
	mov	si,CON(rd_gdt)
	mov	ax,CON(0x8700)		; have the BIOS move the memory
	int	0x15
	pop	ax			; restore register
	mov	si,CON(movmsg)
	jc	mvret			; oops, something went wrong...
mvzero:	or	al,al
	jz	mvno64
mvloop:	dec	BLOC(rd_srcb+2)		; decrement source address
	dec	BLOC(rd_dstb+2)		; decrement dest address
	mov	cx,CON(0x8000)		; move in chunks of 64k/2 words
	push	ax			; save register
	push	cs			; pass gdt in es:si
	pop	es
	mov	si,CON(rd_gdt)
	mov	ax,CON(0x8700)
	int	0x15
	pop	ax
	mov	si,CON(movmsg)
	jc	mvret			; oops, something went wrong...
	dec	al
	jnz	mvloop
mvno64:	xor	si,si
mvret:	pop	di			; restore all registers
	pop	es
	pop	bx
	pop	ax
	ret				; done



;====================================================================
;
; Initialize the information about the kernel image and ramdisk image that
; is stored in the header of the kernel setup code. If an error occurred,
; SI contains a pointer to the error message.
;
; Changed registers: AX, BX, CX, DX, SI, ES, DI

initsetup:
	mov	si,CON(seterr)
	les	di,LOC(kernel)		; get address of kernel setup header
	SEGES
	mov	ax,[di+SU_MAGIC_ADDR]	; check magic number
	cmp	ax,CON(SU_MAGIC_L)
	jne	iserr
	SEGES
	mov	ax,[di+SU_MAGIC_ADDR+2]
	cmp	ax,CON(SU_MAGIC_H)
	jne	iserr
	mov	si,CON(oldkrn)
	SEGES
	mov	ax,[di+SU_VERSION_ADDR]	; check version number
	cmp	ax,CON(SU_VERSION)
	jb	iserr
	SEGES				; this is "mknbi"
#ifdef	USE_AS86
	mov	byte ptr [di+SU_TYPE_OF_LOADER],BCON(SU_MY_TYPE_OF_LOADER)
#endif
#ifdef	USE_NASM
	mov	BLOC(di+SU_TYPE_OF_LOADER),BCON(SU_MY_TYPE_OF_LOADER)
#endif
	mov	ax,LOC(lflags)
	SEGES
	mov	[di+SU_LOAD_FLAGS],ax	; can use heap, might be loaded high
	mov	ax,LOC(code32+0)
	SEGES
	mov	[di+SU_32_START],ax	; update 32bit kernel entry point
	mov	ax,LOC(code32+2)	; if loaded into high memory
	SEGES
	mov	[di+SU_32_START+2],ax
	mov	ax,LOC(ramdisk+0)
	SEGES
	mov	[di+SU_RAMDISK_START],ax	; tell the kernel about the ramdisk
	mov	ax,LOC(ramdisk+2)
	SEGES
	mov	[di+SU_RAMDISK_START+2],ax
	mov	ax,LOC(rdsize+0)
	SEGES
	mov	[di+SU_RAMDISK_SIZE],ax
	mov	ax,LOC(rdsize+2)
	SEGES
	mov	[di+SU_RAMDISK_SIZE+2],ax
	SEGES				; kernel may use the heap
	mov	[di+SU_HEAP],WCON(SU_HEAP_END)
	JMP(isok)

iserr:	mov	ax,LOC(ramdisk+0)	; cannot load ramdisk with old
	or	ax,LOC(ramdisk+2)	; kernels
	jnz	isret
	mov	ax,LOC(lflags)
	test	al,CON(1)		; cannot load high with old kernels
	jnz	isret
isok:	xor	si,si
isret:	ret



;====================================================================
;
; Handle any "vga=" argument that might be on the command line
;
; Changed registers: AX, BX, CX, DX, SI, DI, ES

vga:	mov	si,CON(vgastr)
	mov	cx,CON(vgaend - vgastr)
	les	di,LOC(cmdptr)
	call	strstr			; ES:DI  = strstr(cmdptr,"vga=")
	jcxz	isret			; not found
	push	di
 	add	di,cx			; ES:DI += strlen("vga=")
	xor	bx,bx

	cld
	mov	ax,di
	mov	cx,CON(askend - askstr)	; strncmp(ES:DI,"ASK",3)
	mov	si,CON(askstr)
	repe
	cmpsb
	mov	di,ax
	jz	doask

	mov	cx,CON(extend - extstr)	; strncmp(ES:DI,"EXTENDED",8)
	mov	si,CON(extstr)
	repe
	cmpsb
	mov	di,ax
	jz	doext

	mov	cx,CON(nrmend - nrmstr)	; strncmp(ES:DI,"NORMAL",6)
	mov	si,CON(nrmstr)
	repe
	cmpsb
	mov	di,ax
	jz	donrm

	mov	cx,CON(hexend - hexstr)	; strncmp(ES:DI,"0x",2)
	mov	si,CON(hexstr)
	repe
	cmpsb
	mov	di,ax
	jz	dohex

	JMP(dodez)

doask:	dec	bx			; ASK      == -3
doext:	dec	bx			; EXTENDED == -2
donrm:	dec	bx			; NORMAL   == -1
vgaset:	les	si,LOC(flopptr)
	SEGES
	mov	[si+VGA_OFFSET],bx

#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
	push	bx
	mov	si,CON(vgastr)
	call	prnstr
	pop	ax
	call	prnwrd
	mov	si,CON(crlf)
	call	prnstr
#endif

	les	di,LOC(cmdptr)		; remove this argument from commandline
	pop	di
	call	rmarg
	JMP(vga)			; handle next argument (if any)

dohex:	mov	cl,CON(4)
	xor	dh,dh
	inc	di
hexloop:inc	di			; ES:DI++
	SEGES
	mov	dl,[di]
	sub	dl,CON(0x30)		; if (*ES:DI < '0') break
	jc	vgaset
	cmp	dl,CON(0x09)		; if (*ES:DI > '9') goto nodigit
	jbe	hexinc
	sub	dl,CON(0x11)		; if (*ES:DI < 'A') break
	jc	vgaset
	cmp	dl,CON(0x05)		; if (*ES:DI > 'F') goto nocapital
	jbe	hexxinc
	sub	dl,CON(0x20)		; if (*ES:DI < 'a') break
	jc	vgaset
	cmp	dl,CON(0x05)		; if (*ES:DI > 'f') break
	ja	vgaset
hexxinc:add	dl,CON(0x0A)
hexinc:	shl	bx,cl			; BX = 16*BX + digit
	add	bx,dx
	JMP(hexloop)

vgasgn:	or	al,al			; if (sign) BX = -BX
	jz	vgaset
	neg	bx
	JMP(vgaset)

dodez:	xor	al,al
	xor	dh,dh
	SEGES
#ifdef	USE_AS86
	cmp	byte ptr [di],BCON(0x2D)	; if (*ES:DI == '-') sign++
#endif
#ifdef	USE_NASM
	cmp	BLOC(di),BCON(0x2D)	; if (*ES:DI == '-') sign++
#endif
	jne	dezcnv
	inc	al			; AL = sign
dezlop:	inc	di			; ES:DI++
dezcnv:	SEGES
	mov	dl,[di]
	sub	dl,CON(0x30)		; if (*ES:DI < '0') break
	jc	vgasgn
	cmp	dl,CON(0x9)		; if (*ES:DI > '9') break
	ja	vgasgn
	add	bx,bx			; BX = 10*BX + *ES:DI - '0'
	mov	cx,bx
	add	bx,bx
	add	bx,bx
	add	bx,cx
	add	bx,dx
	JMP(dezlop)



;====================================================================
;
; Process kernel parameters in vendor extension tags
;
; Changed registers: AX, BX, CX, DX, SI, DI, ES

vparms:	mov	al,CON(BOOTP_RFC_VID)	; check for 'äEth' vendor ID
	call	gettag
	cmp	cx,BCON(6)		; there have to be at least 6 bytes
	jb	vperr1			; in this tag
	SEGES
	cmp	[di+0],WCON(BOOTP_VID_1)
	jnz	vperr0
	SEGES
	cmp	[di+2],WCON(BOOTP_VID_2)
vperr0:	jnz	vperr2

	mov	al,CON(BOOTP_RFC_VSEL)	; get user selection
	call	gettag
	cmp	cx,BCON(1)
vperr1:	jb	vperr
	SEGES
	mov	al,[di+0]		; get selected image descripton
	call	gettag
	jcxz	vperr

	cld
	SEGES
	mov	cl,[di-1]
	mov	bx,CON(6)		; scan for parameter field
	mov	ax,CON(0x3A)		; count colons
vplop1:	repne
	scasb
	jcxz	vperr
	dec	bx
	jnz	vplop1

	mov	si,di
	repne				; scan for end of parameter field
	scasb
	SEGES
	cmp	[di-1],al
	jnz	vnocol
	dec	di
vnocol:	mov	cx,di			; compute length of parameter field
	sub	cx,si
	mov	bx,LOC(cmdlen)
	add	bx,cx
	add	bx,BCON(2)
	cmp	bx,LOC(cmdmax)
	jae	vperr
	dec	bx
	mov	ax,es
	les	di,LOC(cmdptr)
	add	di,LOC(cmdlen)
	push	ds
	mov	ds,ax
	mov	al,CON(0x20)		; add a blank
	stosb
vpcpy:	lodsb
	or	al,al
	jnz	vnonul
	sub	bx,cx
	JMP(vpeos)

vnonul:	cmp	al,CON(0x7E)		; check for tilde character
	jnz	vnoesc
	dec	bx
	dec	cx
	jz	vpeos
	lodsb
	cmp	al,CON(0x62)		; '~b' -> '\\'
	jb	vnoesc
	jnz	vpnob
	mov	al,CON(0x5C)
	JMP(vnoesc)

vperr2:	JMP(vperr)

vpnob:	cmp	al,CON(0x63)		; '~c' -> ':'
	jnz	vnoesc
	mov	al,CON(0x3A)
vnoesc:	stosb
	loop	vpcpy			; loop over whole string

vpeos:	xor	al,al
	stosb				; add trailing 0
	pop	ds
	mov	LOC(cmdlen),bx
vperr:	ret



;====================================================================
;
; String and constants definitions


; Startup signature

sigmsg:	db	0x0D, 0x0A
	STRDECL("Linux Net Boot Image Loader ")
	STRDECL(VERSION)
	db	0x0D, 0x0A
	STRDECL("Copyright (C) 1996,1997 G. Kuhlmann and M. Gutschke")
	db	0x0D, 0x0A
	STRDECL(COPYRIGHT)
	db	0x0D, 0x0A
crlf:	db	0x0D, 0x0A
	db	0


; Magic numbers for boot record and bootp entry

bmagic:	dd	BOOT_MAGIC		; boot image magic number
vmagic:	STRDECL(VENDOR_MAGIC		; vendor magic ID)
	db	0			; end of vendor magic ID
pmagic:	db	BOOTP_MAGIC_RFC		; bootp magic ID for RFC 1048
	db	BOOTP_MAGIC_CMU		; bootp magic ID for CMU
	db	BOOTP_MAGIC_STA		; bootp magic ID for Stanford
	db	0


; Possible option names for command line

rtstr:	STRDECL("nfsroot=")
rtend:	db	0

adrstr:	STRDECL("ip=")
adrend:	db	0

vgastr:	STRDECL("vga=")
vgaend:	db	0

askstr:	STRDECL("ASK")
askend:	db	0

extstr:	STRDECL("EXTENDED")
extend:	db	0

nrmstr:	STRDECL("NORMAL")
nrmend:	db	0

hexstr:	STRDECL("0x")
hexend:	db	0


nulstr:	db	0

; Possible arguments for command line options

romstr:	STRDECL("rom")
romend:	db	0

kerstr:	STRDECL("kernel")
kerend:	db	0


; Error messages

memerr:	STRDECL("Not enough memory")
	db	0x0D, 0x0A
	db	0

recerr:	STRDECL("Error in load record data")
	db	0x0D, 0x0A
	db	0

btperr:	STRDECL("BOOTP record too large")
	db	0x0D, 0x0A
	db	0

seterr:	STRDECL("Error in kernel setup header")
	db	0x0D, 0x0A
	db	0

bmerr:	STRDECL("Invalid boot header magic number")
	db	0x0D, 0x0A
	db	0

vmerr:	STRDECL("Invalid vendor magic ID")
	db	0x0D, 0x0A
	db	0

oldkrn:	STRDECL("Linux kernel too old for initrd/bzImage")
	db	0x0D, 0x0A
	db	0

movmsg:	STRDECL("Failed to move ramdisk image to end of memory")
	db	0x0D, 0x0A
	db	0


#if defined(ASM_DEBUG) || defined(ASM_DEBUG_VERBOSE)
; Some debug messages

ad1msg:	STRDECL("Header addr = ")
	db	0

ad2msg:	STRDECL("BOOTP addr = ")
	db	0

pthmsg:	STRDECL("Using boot file path name")
	db	0x0D, 0x0A, 0

nobmsg:	STRDECL("BOOTP record does not contain valid vendor ID")
	db	0x0D, 0x0A, 0

btpmsg:	STRDECL("Using bootp record, vendor ID is ")
	db	0

kermsg:	STRDECL("Kernel start: ")
	db	0

rdmsg:	STRDECL("Ramdisk start: ")
	db	0

cd32msg:STRDECL("Kernel image: ")
	db	0

dynmsg:	STRDECL("Dynamic memory start: ")
	db	0

stamsg:	STRDECL("Static memory start: ")
	db	0

cmdmsg:	STRDECL("Address of command line: ")
	db	0
#endif



;====================================================================
;
; Variable definitions

header:	dd	0			; pointer to boot header from boot rom
bootp:	dd	0			; pointer to bootp block from boot rom
kernel:	dd	0			; pointer to kernel entry
code32:	dd	0			; pointer to kernel start (32bit)
lflags:	dw	0			; load flags

oldDS:	dw	0			; old DS from boot rom
oldES:	dw	0			; old ES from boot rom
oldBP:	dw	0			; old BP from boot rom
oldSI:	dw	0			; old SI from boot rom
oldDI:	dw	0			; old DI from boot rom

avlmem:	dw	0			; available amount of RAM in kB
extmem:	dw	0			; extended memory in kB

ramdisk:dd	0			; pointer to start of ramdisk (32bit)
rdsize:	dd	0			; size of ramdisk

cmdptr:	dd	0			; pointer to start of command line
cmdlen:	dw	0			; length of command line
cmdmax:	dw	0			; maximum length of command line

botptr:	dd	0			; pointer to bootp record
botlen:	dw	0			; length of bootp record
botid:	dw	0			; vendor magic ID (offset into table)

pthptr:	dd	0			; pointer to path string
pthlen:	dw	0			; length of path string

adrptr:	dd	0			; pointer to IP address string
adrlen:	dw	0			; length of IP address string

flopptr:dd	0			; pointer to floppy boot sector

rd_gdt:	dw	0,0,0,0
	dw	0,0,0,0
rd_src:	dw	0xffff			; length
rd_srcb:db	0,0,0			; base
	db	0x93			; typebyte
	dw	0			; limit16,base24 =0
rd_dst:	dw	0xffff			; length
rd_dstb:db	0,0,0			; base
	db	0x93			; typebyte
	dw	0			; limit16,base24 =0
	dw	0,0,0,0			; BIOS CS
	dw	0,0,0,0			; BIOS DS

