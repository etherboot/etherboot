; rmrd.S  -  DOS program to remove ramdisk
;
; Copyright (C) 1996-1998 Gero Kuhlmann   <gero@gkminix.han.de>
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
#define	JMP(x)		jmp x
#define	STRDECL(s)	.ascii	s
#endif

#ifdef	USE_NASM
#define	CON(x)		x
#define	BCON(x)		byte x
#define	JMP(x)		jmp short x
#define	STRDECL(s)	db	s
#endif

#ifdef	USE_AS86
	.text
	entry	start
#endif
#ifdef	USE_NASM
	text
#endif

	org	0x100
start:	push	cs
	pop	ds
	xor	ax,ax
	mov	es,ax
	mov	si,CON(rdsig)
	mov	di,CON(0xF1) * 4	; check ramdisk driver signature
	mov	cx,CON(4)
	repz
	cmpsb
	jnz	start4

	mov	ax,CON(0x9C00)
	int	0xF8			; check if a ramdisk driver is
	cmp	ax,CON(0x009C)		; installed
	je	start1
start4:	mov	al,BCON(1)
	mov	dx,CON(noramd)
	JMP(start9)

start1:	mov	ax,CON(0x9C03)
	int	0xF8			; get ramdisk ID
	or	cl,cl			; has to be drive A: (e.g. floppy)
	jz	start2
	mov	al,BCON(2)
	mov	dx,CON(noflop)
	JMP(start9)

start2:	mov	ax,CON(0x9C04)
	int	0xF8			; remove ramdisk
	or	al,al
	jz	start3
	mov	al,BCON(3)
	mov	dx,CON(rderr)
	JMP(start9)

start3:	xor	al,al
	mov	dx,CON(rdok)
start9:	push	ax
	mov	ah,BCON(0x09)
	int	0x21			; print error message
	mov	dx,CON(crlf)
	mov	ah,BCON(0x09)
	int	0x21			; print CR/LF
	pop	ax
	mov	ah,BCON(0x4C)
	int	0x21			; terminate program

; Data area

rdsig:	STRDECL("NetB")
noramd:	STRDECL("No ramdisk found$")
noflop:	STRDECL("Ramdisk is not drive A:$")
rderr:	STRDECL("Can't remove ramdisk$")
rdok:	STRDECL("Ramdisk removed$")
crlf:	db	0x0D,0x0A
	STRDECL("$")
