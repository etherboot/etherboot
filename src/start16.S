/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

/**************************************************************************
START - Where all the fun begins....
**************************************************************************/
	.globl	_start
_start:
	mov	initsp,sp
	call	_main
	.globl	_exit
_exit:
;	we reset sp to the location just before entering main
;	instead of relying on the return from main because exit
;	could have been called from anywhere
	mov	sp,initsp
	retf

/**************************************************************************
CURRTICKS - Get Time
Use direct memory access to BIOS variables, longword 0040:006C (ticks
today) and byte 0040:0070 (midnight crossover flag) instead of calling
timeofday BIOS interrupt.
**************************************************************************/
	.globl	_currticks
_currticks:
	push	bx
	push	ds
	mov	ax,*0x40
	mov	ds,ax
	mov	bl,0x70			;get midnight crossover flag
	mov	0x70,*0			;clear flag
	lds	ax,0x6C			;get 32 bit timeofday in one hit
	mov	dx,ds			;high 16 bits -> dx
	pop	ds			;restore ds for accessing data
	cmpb	bl,*0
	je	notmidnite
	add	dayslo,*0x00b0		;0x1800b0 ticks per day
	adc	dayshi,*0x18
notmidnite:
	add	ax,dayslo
	adc	dx,dayshi
	pop	bx
	ret

/**************************************************************************
PUTCHAR - Print a character
**************************************************************************/
	.globl	_putchar
_putchar:
	push	bp
	mov	bp,sp
	push	cx
	push	bx
	movb	cl,4[bp]
	cmpb	cl,*0x0a	/* newline? */
	jne	putchar1
	mov	bx,*1
	movb	ah,*0x0e
	movb	al,*0x0d	/* CR first */
	int	0x10
putchar1:
	mov	bx,*1
	movb	ah,*0x0e
	movb	al,cl
	int	0x10
	pop	bx
	pop	cx
	pop	bp
	ret

/**************************************************************************
GETCHAR - Get a character from the console WITHOUT ECHO
**************************************************************************/
	.globl	_getchar
_getchar:
	push	bx
	movb	ah,*0x0
	int	0x16
	cmpb	al,*0x0d	; \r?
	jne	notcr
	movb	al,*0x0a	; substitute \n
notcr:
	movb	bl,al
	xor	ax,ax
	movb	al,bl
	pop	bx
	ret

/**************************************************************************
ISKEY - Check for keyboard interrupt
**************************************************************************/
	.globl	_iskey
_iskey:
	push	bx
	xor	bx,bx
	movb	ah,*0x1
	int	0x16
	jz	iskey1
	movb	bl,al
iskey1:
	xor	ax,ax
	movb	al,bl
	pop	bx
	ret

/**************************************************************************
MEMSIZE - Determine size of extended memory
**************************************************************************/

	.globl	_memsize
_memsize:
	push	bx
	mov	ax,*0x8800
	int	0x15
	mov	bx,ax
	xor	ax,ax
	mov	ax,bx
	pop	bx
	ret

/**************************************************************************
START_PROG - Call program code
**************************************************************************/
	.globl	_start_linux
_start_linux:
	call	sl1
	ret
sl1:	jmpf	*0x0,*0x9020

	.globl	_xstart
;
;	usage: xstart(execaddr, headeraddr, bootpaddr);
;			4,6	8,10		12,14
;
_xstart:
	push	bp
	mov	bp,sp
	push	bx
	push	cx
	mov	ax,14[bp]
	push	ax	/* bootp record */
	mov	ax,12[bp]
	push	ax
	mov	ax,10[bp]
	push	ax	/* file header */
	mov	ax,8[bp]
	push	ax
	mov	ax,cs
	push	ax	/* return address */
	mov	ax,*xs1-_start
	push	ax
	mov	ax,6[bp]
	push	ax	/* exec addr */
	mov	ax,4[bp]
	push	ax
	retf		/* jump intersegment using a return */
xs1:	pop	bp
	pop	cx
	pop	bx
	ret

; int inw(int port);
; reads a word from the i/o port  port  and returns it

	.globl	_inw
_inw:
	pop	bx
	pop	dx
	dec	sp
	dec	sp
	inw
	jmp	bx

; int inb(int port);
; reads a byte from the i/o port  port  and returns it

	.globl	_inb,_inb_p
_inb:
_inb_p:
	pop	bx
	pop	dx
	dec	sp
	dec	sp
	inb
	sub	ah,ah
	jmp	bx

; int insw(int port, char *dst, int nwords);
; block read from i/o port

	.globl	_insw
_insw:
	pop	bx		;return address
	pop	dx		;port
	pop	di		;destination
	pop	cx		;count
inswloop:
	jcxz	inswret
	inw
	mov	[di],ax
	inc	di
	inc	di
	dec	cx
	jmp	inswloop
inswret:
	sub	sp,*6		;precompensate for add in caller
	jmp	bx

; Note: this will not work on 8086. Affected driver is NI5010.
; int insb(int port, char *dst, int nwords);
; block read from i/o port

	.globl	_insb
_insb:
	push	ds
	pop	es		; ds -> es
	pop	bx		;return address
	pop	dx		;port
	pop	di		;destination
	pop	cx		;count
	cld
	rep
	insb
	sub	sp,*6		;precompensate for add in caller
	jmp	bx

; void outw(int value, int port);
; writes the word  value  to  the i/o port  port

	.globl	_outw
_outw:
	pop	bx
	pop	ax
	pop	dx
	sub	sp,*4
	outw
	jmp	bx

; void outb(char value, int port);
; writes the byte  value  to  the i/o port  port

	.globl	_outb,_outb_p
_outb:
_outb_p:
	pop	bx
	pop	ax
	pop	dx
	sub	sp,*4
	outb
	jmp	bx

; int outsw(int port, char *src, int nwords);
; block write from i/o port

	.globl	_outsw
_outsw:
	pop	bx		;return address
	pop	dx		;port
	pop	di		;destination
	pop	cx		;count
outswloop:
	jcxz	outswret
	mov	ax,[di]
	outw
	inc	di
	inc	di
	dec	cx
	jmp	outswloop
outswret:
	sub	sp,*6		;precompensate for add in caller
	jmp	bx

; Note: this will not work on 8086. Affected driver is NI5010.
; int outsb(int port, char *src, int nwords);
; block write from i/o port

	.globl	_outsb
_outsb:
	pop	bx		;return address
	pop	dx		;port
	pop	si		;destination
	pop	cx		;count
	cld
	rep
	outsb
	sub	sp,*6		;precompensate for add in caller
	jmp	bx

;*************************************************************************
;	longswap
;	swap the bytes of a long integer from pc
;	order (reverse) to in-order.  this will work both ways.
;	returns the new long value
;	usage:
;	l2 = htonl/ntohl(l)
;
	.globl	_htonl,_ntohl
_htonl:
_ntohl:
	push	bp
	mov	bp,sp
	mov	ax,6[bp]	;high bytes of the long int
	mov	dx,4[bp]	;low bytes of the long int
;
;	get the data
;
	xchgb	al,ah		;swap them, these are now low
	xchgb	dl,dh		;swap the others
	pop	bp
	ret
;
;*************************************************************************
;	intswap
;	swap the bytes of an integer, returns the swapped integer
;
;	usage: i = htons/ntohs(i);
;
	.globl	_htons,_ntohs
_htons:
_ntohs:
	push	bp
	mov	bp,sp
	mov	ax,4[bp]
	xchgb	al,ah
	pop	bp
	ret

;	lintoseg
;	extracts the segment from a linear address in bx,ax
;	returns segment in ax
;
lintoseg:
	and	ax,*0xfff0		;clear bottom 4 bits
	clc
	rcr	bx,*1			;and shift segment 4 bits down
	rcr	ax,*1
	rcr	bx,*1
	rcr	ax,*1
	rcr	bx,*1
	rcr	ax,*1
	rcr	bx,*1
	rcr	ax,*1
	ret

;
;*************************************************************************
;	memcpyf
;	like memcpy, but copies intersegment
;
;	usage: memcpyf(fp, p, n);
;
	.globl	_memcpyf
_memcpyf:
	push	bp
	mov	bp,sp
	push	di
	push	si
	push	es
	cld
;	4,6[bp] = fp, 8[bp] = p, A[bp] = n
	mov	si,8[bp]		;p
	mov	ax,4[bp]		;fp low word
	mov	di,ax
	and	di,*0xF			;keep LS 4 bits
	mov	cx,0xA[bp]
	mov	bx,6[bp]
	cmp	bx,*0x10		;above 1Mb?
	jae	memcpyf2		;move to extended memory
	call	lintoseg
	mov	es,ax
	shr	cx,*1
	rep
	movsw
	jnc	memcpyf1
	movsb
memcpyf1:
	pop	es
	pop	si
	pop	di
	pop	bp
	ret

/*
 *	Copy to extended memory adapted from utility routine in netboot
 *	by Gero Kuhlmann
 */

memcpyf2:
	mov	word ptr [gdt_table + 3 * gdt_size + gdt_base + 0],ax
	mov	byte ptr [gdt_table + 3 * gdt_size + gdt_base + 2],bl
	mov	byte ptr [gdt_table + 3 * gdt_size + gdt_flags],*gdt_stdflag
	mov	word ptr [gdt_table + 3 * gdt_size + gdt_length],cx
	mov	word ptr [gdt_table + 3 * gdt_size + gdt_res],*0
	mov	ax,ds			;segment
	rol	ax,*1			;rotate so ax=[bottom 12,top 4] bits
	rol	ax,*1			;even though XMS will fail on the 8086
	rol	ax,*1			;we do not use the rol ax,*n instruction
	rol	ax,*1			;so we do not hang the processor
	mov	bx,ax
	and	ax,*0xFFF0		;ax = bottom 12 bits of segment << 4
	and	bx,*0x000F		;bx = top 4 bits of segment
	add	ax,si			;offset + segment x 16
	adc	bx,*0			;carry into top 4 bits
	mov	word ptr [gdt_table + 2 * gdt_size + gdt_base + 0],ax
	mov	byte ptr [gdt_table + 2 * gdt_size + gdt_base + 2],bl
	mov	byte ptr [gdt_table + 2 * gdt_size + gdt_flags],*gdt_stdflag
	mov	word ptr [gdt_table + 2 * gdt_size + gdt_length],cx
	mov	word ptr [gdt_table + 2 * gdt_size + gdt_res],*0
	inc	cx			;convert byte count into word count
	shr	cx,*1
	mov	ax,ds
	mov	es,ax			;es:si -> gdt
	mov	si,*gdt_table
	mov	ah,*0x87
	int	0x15			;call BIOS INT15H to do the copy
	jmp	memcpyf1		;common exit code

;
;*************************************************************************
;	fmemcpy
;	like memcpy, but copies intersegment
;
;	usage: fmemcpy(p, fp, n);
;
	.globl	_fmemcpy
_fmemcpy:
	push	bp
	mov	bp,sp
	push	di
	push	si
	push	es
	cld
	mov	ax,ds
	mov	es,ax
;	4[bp] = p, 6,8[bp] = fp, A[bp] = n
	mov	ax,6[bp]		;fp low word
	mov	si,ax
	and	si,*0xf			;keep LS 4 bits
	mov	bx,8[bp]
	call	lintoseg
	mov	ds,ax
	mov	di,4[bp]		;p
	mov	cx,$A[bp]
	shr	cx,*1
	rep
	movsw
	jnc	fmemcpy1
	movsb
fmemcpy1:
	mov	ax,es
	mov	ds,ax
	pop	es
	pop	si
	pop	di
	pop	bp
	ret

;
;*************************************************************************
;	fpeekw
;	returns one word from a far address
;
;	usage: fpeekw(fp);
;
	.globl	_fpeekw
_fpeekw:
	push	bp
	mov	bp,sp
	push	si
;	4,6[bp] = fp
	mov	ax,4[bp]		;fp low word
	mov	si,ax
	and	si,*0xf			;keep LS 4 bits
	mov	bx,6[bp]
	call	lintoseg
	mov	bx,ax
	mov	ax,[bx+si]
	pop	di
	pop	bp
	ret

;
;*************************************************************************
;	bzerof
;	like bzero, but zeros any segment
;
;	usage: bzerof(fp, n);
;
	.globl	_bzerof
_bzerof:
	push	bp
	mov	bp,sp
	push	di
	push	si
	push	es
	cld
;	4,6[bp] = fp, 8[bp] = n
	mov	ax,4[bp]		;fp low word
	mov	di,ax
	and	di,*0xf			;keep LS 4 bits
	mov	bx,6[bp]
	call	lintoseg
	mov	es,ax
	mov	cx,$8[bp]
	xor	ax,ax
	shr	cx,*1
	rep
	stosw
	jnc	bzerof1
	stosb
bzerof1:
	pop	es
	pop	si
	pop	di
	pop	bp
	ret

;
;*************************************************************************
;	fbsame
;	returns 1 if all bytes from fp[0] to fp[n-1] have the value c
;
;	usage: fbsame(fp, c, n);
;
	.globl	_fbsame
_fbsame:
	push	bp
	mov	bp,sp
	push	di
	push	si
	cld
;	4,6[bp] = fp, 8[bp] = c, A[bp] = n
	mov	ax,4[bp]		;fp low word
	mov	di,ax
	and	di,*0xf			;keep LS 4 bits
	mov	bx,6[bp]
	call	lintoseg
	mov	es,ax
	mov	ax,8[bp]		;c
	mov	cx,$A[bp]
	repe
	scasb
	mov	ax,*0			;not xor because want to preserve z
	jne	fbsameret
	inc	ax
fbsameret:
	pop	si
	pop	di
	pop	bp
	ret

/*
 *	Needed by memcpyf into extended memory
 */

/*
 *	Layout of descriptors in GDT:
 */
gdt_length	equ	0		;length of segment
gdt_base	equ	2		;base of segment
gdt_flags	equ	5		;descriptor flags
gdt_res		equ	6		;reserved
gdt_size	equ	8		;size of descriptor
gdt_stdflag	equ	0x93		;standard flags for GDT entries

	.data
dayslo:	dw	0			;low word of ticks per day
dayshi:	dw	0			;high word

initsp:	dw	0			;sp value before main() was called

	.bss
	.lcomm	gdt_table,6 * gdt_size	;descriptor table for move routine
