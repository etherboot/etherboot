; first.S  -  primary boot loader for DOS
;
; Copyright (C) 1996-1998 Gero Kuhlmann   <gero@gkminix.han.de>
; Modifications for booting FreeDOS by Ken Yap <ken_yap@users.sourceforge.net>
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
#define	LOC(x)		x
#define	BLOC(x)		byte ptr x
#define	WLOC(x)		word ptr x
#define	JMP(x)		jmp x
#define	STRDECL(s)	.ascii	s
#define	SEGCS		seg	cs
#define	SEGES		seg	es
#define	ALIGN(x)	.align	x
#define	SPACE(x)	.space	x
#endif

#ifdef	USE_NASM
#define	CON(x)		x
#define	BCON(x)		byte x
#define	LOC(x)		[x]
#define	BLOC(x)		byte [x]
#define	WLOC(x)		word [x]
#define	JMP(x)		jmp short x
#define	STRDECL(s)	db	s
#define	SEGCS		cs
#define	SEGES		es
#define	ALIGN(x)	align x, db 0
#define	SPACE(x)	times x db 0
#endif

#ifndef ASM_DEBUG
#undef ASM_DEBUG
#endif
#include "first-dos.h"
#include "version-dos.h"

#ifdef	USE_AS86
	.text
	.org	0
#endif
#ifdef	USE_NASM
	text
#endif

	mov	dx,ds
	mov	ax,cs			; set DS
	mov	ds,ax
	mov	LOC(oldES),es
	mov	LOC(oldDS),dx		; save old register values in case
	mov	LOC(oldBP),bp		; we have to return to the boot rom
	mov	LOC(oldSI),si
	mov	LOC(oldDI),di
	mov	bp,sp
	mov	ax,[bp+4]
	mov	LOC(header+0),ax		; load the address of the boot header
	mov	ax,[bp+6]
	mov	LOC(header+2),ax
	mov	ax,[bp+8]
	mov	LOC(bootp+0),ax		; load the address of the bootp block
	mov	ax,[bp+10]
	mov	LOC(bootp+2),ax

; Tell the user who we are and that we started running

	mov	si,CON(sigmsg)
	call	prnstr

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
	jz	getrd			; vendor ID ok, continue
	SEGES
	cmp	al,[bx+di+BOOT_HD_VENDOR]
	jne	doerr1
	inc	di
	JMP(dovmag)

doerr1:	call	prnstr			; in case of error return to the
	mov	si,LOC(oldSI)		; boot rom with registers set
	mov	di,LOC(oldDI)		; correctly
	mov	bp,LOC(oldBP)
	mov	es,LOC(oldES)
	mov	ds,LOC(oldDS)
	retf

; Next get the address of the ramdisk and its size.

getrd:	mov	si,CON(recerr)
	mov	al,CON(VENDOR_RAMDISK)
	call	fndldr				; find load record for ramdisk
	mov	ax,es
	or	ax,di
	jz	doerr1
	SEGES
	mov	al,[di+BOOT_LD_FLAGS]		; get load record flags
	test	al,CON(BOOT_FLAG_B0 + BOOT_FLAG_B1)	; check that it has a
	jnz	doerr1				; correct flag

	SEGES
	mov	ax,[di+BOOT_LD_ADDR+0]		; get base adress of ramdisk
	SEGES
	mov	bx,[di+BOOT_LD_ADDR+2]
	mov	LOC(rdaddr+0),ax
	mov	LOC(rdaddr+2),bx

	SEGES
	mov	ax,[di+BOOT_LD_MLENGTH+0]	; get ramdisk size
	SEGES
	mov	bx,[di+BOOT_LD_MLENGTH+2]
	add	ax,CON(0x03ff)			; round to nearest kb
	adc	bx,BCON(0)
	mov	cl,CON(10)
	shr	ax,cl				; convert into kb
	mov	cl,CON(6)
	shl	bx,cl
	or	ax,bx
	mov	LOC(rdsize),ax

; Get the disk geometry out of the vendor information block in the
; load record

	SEGES
	mov	bl,[di+BOOT_LD_LENGTH]
	and	bl,CON(0x0f)
	xor	bh,bh				; compute pointer to
	shl	bx,CON(1)			; vendor block
	shl	bx,CON(1)
	SEGES
	mov	ax,[di+bx+BOOT_LD_SECNUM]	; get number of sectors
	mov	LOC(secnum),ax
	SEGES
	mov	ax,[di+bx+BOOT_LD_SPT]		; get sectors per track
	mov	LOC(secptk),al
	SEGES
	mov	ax,[di+bx+BOOT_LD_CYL]		; get number of cylinders
	mov	LOC(cylnum),ax
	SEGES
	mov	al,[di+bx+BOOT_LD_NOHD]		; get no-hard-disk flag
	mov	LOC(nohd),al
	SEGES
	mov	al,[di+bx+BOOT_LD_DRIVE]		; get ram disk drive id
	mov	LOC(drvid),al

; Set the address of the BIOS disk status byte

	mov	bx,CON(BIOS_FDSTAT)
	cmp	al,CON(0x80)
	jb	setsts
	mov	bx,CON(BIOS_HDSTAT)
setsts:	mov	LOC(statof),bx

; Get system configuration from BIOS

	push	ax
	int	0x11
	mov	LOC(syscnf),ax
	pop	ax

; Get the number of floppy or hard disk drives in the system and set
; the DOS disk parameter block

	cmp	al,BCON(0x80)
	jae	getnm2

	mov	ah,CON(0x08)
	xor	dl,dl
	int	0x13			; get the number of floppy disk
	jc	getnm1			; drives from the BIOS
	or	dl,dl
	jnz	gotnum
	inc	dl			; indicate at least one drive
	JMP(gotnum)
getnm1:	mov	dx,LOC(syscnf)		; if int 13h didnt work try it with
	test	dl,CON(0x01)		; the mainboard dip switch config
	jz	getnm3
	mov	cl,CON(6)
	shr	dl,cl
	and	dl,CON(0x03)		; determine number of floppy disk
	inc	dl			; drives
	JMP(gotnum)

getnm2:	mov	ah,CON(0x08)
	mov	dl,CON(0x80)
	int	0x13			; get the number of hard disk
	jc	getnm3			; drives from the BIOS
	inc	dl
	JMP(gotnum)
; The next line was mov dl,1 in netboot-0.8.1. This was probably an error.
getnm3:	mov	dl,CON(1)		; we have at least one drive

gotnum:	mov	LOC(drvnum),dl		; save number of disk drives
	call	setdpb			; set disk parameter block

; Now get the boot sector of the ramdisk and check that its correct. If
; we are simulating a hard disk the boot sector contains the partition
; table, which we have to analyze. Then load the partitions boot sector.

	mov	ax,CON(TEMP_SEGMENT)
	mov	es,ax			; pointer to temporary buffer
	xor	bx,bx
	xor	al,al			; indicate read
	xor	dx,dx			; first sector
	call	rwsect			; read boot sector
	mov	si,CON(rderr)
	jc	doerr2
	cmp	BLOC(drvid),CON(0x80)	; if the ram disk is simulates a
	jb	chkbot			; floppy, there is no partition table

	mov	si,CON(dskerr)		; prepare for correct error message
	SEGES
	cmp	BLOC(PART_STATUS),CON(PART_ACTIVE)
	jne	doerr2
	SEGES
	cmp	BLOC(PART_TYPE),CON(PART_FAT16)
	je	partok
	SEGES
	cmp	BLOC(PART_TYPE),CON(PART_FAT12)
	jne	doerr2
partok:	SEGES
	mov	dx,[PART_ABS_SECT+0]	; get number of first sector
	SEGES
	mov	ax,[PART_ABS_SECT+2]
	or	ax,ax
	jnz	doerr2
	xor	al,al			; indicate read
	call	rwsect			; read boot sector
	mov	si,CON(rderr)
	jc	doerr2
#ifndef	HD_PARM_CHECK
	JMP(dobotp)
#endif

chkbot:	mov	si,CON(dskerr)		; prepare for correct error message
	mov	al,LOC(drvid)
	SEGES
	cmp	BLOC(DISK_BOOT),al	; check boot disk number
	jne	doerr2
	SEGES
	cmp	WLOC(DISK_BPS),CON(SECT_SIZE)	; check sector size
	jne	doerr2
	SEGES
	cmp	WLOC(DISK_HEADS),BCON(2)	; check number of heads
	jne	doerr2
	SEGES
	mov	ax,LOC(DISK_SPT)	; check number of sectors per track
	cmp	al,LOC(secptk)
	je	dobotp

doerr2:	call	prnstr			; in case of error return to the
	mov	si,LOC(oldSI)		; boot rom with registers set
	mov	di,LOC(oldDI)		; correctly
	mov	bp,LOC(oldBP)
	mov	es,LOC(oldES)
	mov	ds,LOC(oldDS)
	retf

; Save the BOOTP record for later retrieval by a DOS program.

dobotp:	cld
	xor	dx,dx
	les	di,LOC(bootp)
	mov	ax,es
	or	ax,di
	jz	dobot9
	SEGES
	mov	al,[di+BOOTP_OP]		; op code must indicate reply
	cmp	al,CON(BOOTP_REPLY)
	jne	dobot9			; it isnt
	add	di,CON(BOOTP_VEND)
	mov	bx,di
	mov	si,CON(pmagic)		; compare vendor ID
dobot1:	mov	di,bx
	mov	cx,CON(BOOTP_MAGIC_LEN)
	repe
	cmpsb
	jz	dobot2			; vendor ID is valid
	add	si,cx
#ifdef	USE_AS86
	cmp	byte ptr[si],CON(0)	; check next vendor ID
#endif
#ifdef	USE_NASM
	cmp	byte [si],CON(0)	; check next vendor ID
#endif
	jne	dobot1
dobot9:	JMP(nobot2)			; vendor ID not found

doerr6:	JMP(doerr2)

dobot2:	sub	si,BCON(BOOTP_MAGIC_LEN)
	sub	si,CON(pmagic)
	mov	ax,si
	push	ds
	mov	bx,ds
	mov	es,bx
	mov	di,CON(btpnew)
	lds	si,LOC(bootp)
	mov	bx,si
	mov	dx,CON(BOOTP_SIZE)
	or	ax,ax			; if not RFC vendor ID the bootp
	jnz	dobot7			; record has fixed length

	xor	cx,cx
	add	si,CON(BOOTP_VEND + BOOTP_MAGIC_LEN)
dobot3:	lodsb
	cmp	al,CON(BOOTP_RFC_NOP)	; handle NOP tag
	jnz	dobot4
	inc	cx
	cmp	cx,BCON(16)		; more than 16 NOP tags is VERY unusual
	jae	dobot7			; so the bootp record maybe broken
	JMP(dobot3)			; loop to next tag

nobot2:	JMP(nobotp)

dobot4:	cmp	al,CON(BOOTP_RFC_END)	; handle END tag
	jnz	dobot6
	mov	dx,si
	sub	dx,bx			; compute length of bootp record
	cmp	dx,CON(BOOTP_SIZE)
	jae	dobot7
	mov	dx,CON(BOOTP_SIZE)	; use minimum size
	JMP(dobot7)
dobot6:	lodsb				; handle all other tags
	mov	cl,al
	xor	ch,ch
	add	si,cx			; jump to next tag
	xor	cx,cx			; reset NOP counter
	JMP(dobot3)			; proceed with next tag

dobot7:	mov	si,CON(btperr)
	mov	ax,CON(btpnew)		; bootp record cannot be larger
	add	ax,dx			; than the current segment
	jc	doerr6
	mov	cx,dx
	mov	si,bx			; restore source pointer
	rep
	movsb				; save the bootp record
	pop	ds
nobotp:	mov	LOC(btplen),dx		; set length of bootp record

; Everything is right, so we can now move the resident section to the
; end of the conventional memory, thus overwriting the bootrom data
; area. Therefore there is no chance of returning to the bootrom from
; now on.
; Note that the resident section doesnt start at offset 0, so we have
; to set the segment address to somewhere lower.

#ifdef ASM_DEBUG
	mov	si,CON(debug1)
	call	prnstr
#endif
	cli
	mov	ax,CON(TEMP_SEGMENT) 	; set new stack
	mov	ss,ax
	mov	sp,CON(0xFFF0)

	cld	
	int	0x12			; get memory size in kB
#ifdef	FREEDOS
	push	ax			; save mem size in kB
#endif	/* FREEDOS */
	mov	cl,CON(6)
	shl	ax,cl			; compute last usable segment
	mov	bx,CON(btpnew)
	add	bx,LOC(btplen)
	mov	dx,bx
	mov	cl,CON(4)
	shr	bx,cl			; compute size of code segment in
	inc	bx			; paragraphs
#ifdef	FREEDOS
	push	bx			; save size in paragraphs
#endif	/* FREEDOS */
	sub	ax,bx			; compute new code segment
	mov	LOC(resseg),ax
	mov	es,ax			; set source and destination ptrs
	mov	si,CON(start_resident)
	mov	di,si
	mov	cx,dx
	sub	cx,si			; compute size of resident area
	rep
	movsb				; move it

#ifdef	FREEDOS
; New code for FreeDOS, adjust the value of the top of memory returned by
; int 0x12. Currently there is no code to restore the original size
	pop	bx			; restore size in paragraphs
	add	bx,BCON(63) 		; round up to next kB
	mov	cl,CON(6)		; divide by 64
	shr	bx,cl
	pop	ax			; restore size in kB
	sub	ax,bx
	mov	bx,CON(0x40)
	mov	es,bx
	SEGES
	mov	LOC(0x13),ax		; store at 0x40:0x13 for int 12h
; End of new code
#endif	/* FREEDOS */

; Setup all interrupt vectors

	mov	bx,LOC(resseg)
	push	ds
	mov	ds,bx
	xor	ax,ax
	mov	es,ax
	SEGES
	mov	ax,LOC(I13_INT+0)
	mov	LOC(old13h+0),ax
	SEGES
	mov	ax,LOC(I13_INT+2)
	mov	LOC(old13h+2),ax
	SEGES
	mov	ax,LOC(I2F_INT+0)
	mov	LOC(old2Fh+0),ax
	SEGES
	mov	ax,LOC(I2F_INT+2)
	mov	LOC(old2Fh+2),ax
	SEGES
	mov	ax,LOC(IF1_INT+0)
	mov	LOC(oldF1h+0),ax
	SEGES
	mov	ax,LOC(IF1_INT+2)
	mov	LOC(oldF1h+2),ax
	SEGES
	mov	ax,LOC(IF8_INT+0)
	mov	LOC(oldF8h+0),ax
	SEGES
	mov	ax,LOC(IF8_INT+2)
	mov	LOC(oldF8h+2),ax
	pop	ds

	SEGES
	mov	LOC(I13_INT+2),bx	; interrupt vector 13h
	SEGES
	mov	WLOC(I13_INT+0),CON(int13)
	SEGES
	mov	LOC(I2F_INT+2),bx	; interrupt vector 2Fh
	SEGES
	mov	WLOC(I2F_INT+0),CON(int2F)
	SEGES
	mov	LOC(IF8_INT+2),bx	; interrupt vector F8h
	SEGES
	mov	WLOC(IF8_INT+0),CON(intF8)
	mov	di,CON(IF1_INT)
	mov	si,CON(if1sig)		; interrupt vector F1h
	mov	cx,CON(4)		; contains the string "NetB"
	rep				; to provide as an installation
	movsb				; check
	sti

; Output some debugging messages by simply calling the interrupt vectors
; which we just created.

#ifdef DRV_DEBUG
	mov	ax,CON(0x4A06)
	int	0x2F
	mov	si,CON(consz)
	call	prnstr			; print out amount of conventional
	mov	ax,dx			; memory
	mov	cl,CON(12)
	shr	ax,cl
	call	prnwrd
	mov	ax,dx
	mov	cl,CON(4)
	shl	ax,cl
	call	prnwrd
	mov	si,CON(bytes)
	call	prnstr

	mov	ax,CON(0x8800)
	int	0x15
	push	ax
	mov	si,CON(extsz)
	call	prnstr			; print out amount of extended memory
	mov	cl,CON(6)
	shr	ax,cl
	call	prnwrd
	pop	ax
	mov	cl,CON(10)
	shl	ax,cl
	call	prnwrd
	mov	si,CON(bytes)
	call	prnstr
#endif

#ifndef	FREEDOS
; The boot sector had to be placed into a temporary memory area to
; avoid overwriting any bootrom structures. However, a call back
; to the bootrom is no longer possible, so we can move the bootblock
; where it belongs.

	push	ds
	mov	ax,CON(TEMP_SEGMENT)
	mov	ds,ax
	xor	ax,ax
	mov	es,ax
	xor	si,si
	mov	di,CON(BOOT_OFFSET)
	mov	cx,CON(SECT_SIZE)
	rep
	movsb				; move it
	pop	ds

; Finally call the boot sector
#else
; Finally call kernel.sys
#endif	/* FREEDOS */

#ifdef ASM_DEBUG
	mov	si,CON(debug2)
	call	prnstr
#endif
#ifdef	FREEDOS
	mov	bl,LOC(drvid)		; FreeDOS gets boot drive from bl, 0=A
#endif
#ifdef ASM_FREEZE_AFTER_INIT
lop:	JMP(lop)
#else
#ifdef	FREEDOS
	jmp	FDKSEG:0		; FreeDOS kernel.sys entry point
#else
	jmp	0:BOOT_OFFSET
#endif	/* FREEDOS */
#endif



;====================================================================
;
; Setup DOS drive parameter block (DPB) for floppy disk drive. The
; DPB is required to activate the floppy disk drive after the ramdisk
; has been turned off.
; Input:  none
; Output: none
; Registers changed: AX, BX, CX, DX

setdpb:	push	es
	push	si
	push	di
	mov	dl,LOC(drvid)
	cmp	dl,CON(0x80)		; can only restore floppy drives
	jae	setdp8

; First get the drive parameters from the BIOS

	mov	LOC(dpb_phys),dl	; set physical drive ID
	mov	ah,CON(0x08)		; get drive parameters from BIOS
	int	0x13
	jc	setdp8
	xor	ah,ah
	mov	al,ch			; set max number of cylinders
	inc	ax
	mov	LOC(dpb_cyls),ax
	mov	si,CON(dpb_bpb_cur)
	mov	al,cl			; set max number of sectors per track
	mov	[si+BPB_SPT],ax
	mov	al,dh
	inc	ax			; set max number of heads
	mov	[si+BPB_HEADS],ax

; Determine DOS disk parameters by drive type

	cmp	bl,CON(5)
	jb	setdp1			; check for invalid drive type
	mov	bl,CON(4)
setdp1:	xor	bh,bh
	dec	bx
	push	bx
	shl	bx,CON(1)
	shl	bx,CON(1)		; compute address into drive para-
	add	bx,CON(drvtab)		; meter table
	xor	ah,ah
	mov	al,[bx+0]		; get # of entries in root dir
	mov	[si+BPB_DIR],al
	mov	al,[bx+1]		; get # of sectors per FAT
	mov	[si+BPB_SPF],ax
	mov	al,[bx+2]		; get # of sectors per cluster
	mov	[si+BPB_SPC],al
	mov	al,[bx+3]		; get media ID
	mov	[si+BPB_MEDIA_ID],al
	pop	bx
	mov	al,[bx+typtab]		; get drive type
	mov	LOC(dpb_type),al

; Determine number of bytes per sector

	SEGES
	mov	cl,[di+3]		; get shift value from BIOS media
	mov	ax,CON(128)		; parameter table
	shl	ax,cl			; shift base value
	mov	[si+BPB_BPS],ax
	JMP(setdp4)
setdp8:	JMP(setdp9)			; needed for short jumps

; Determine total number of sectors

setdp4:	mov	ax,[si+BPB_SPT]
	mul	WLOC(dpb_cyls)
	or	dx,dx			; should not overflow
	jnz	setdp8
#ifdef	USE_AS86
	cmp	[si+BPB_HEADS],BCON(2)
#endif
#ifdef	USE_NASM
	cmp	word [si+BPB_HEADS],BCON(2)
#endif
	jb	setdp3
	shl	ax,CON(1)
	jc	setdp8
setdp3:	mov	[si+BPB_TOT_SECTS],ax

; Determine if the drive can detect disk changes

	mov	ah,CON(0x15)
	mov	dl,LOC(drvid)
	int	0x13			; get DASD type from BIOS
	mov	bl,ah
	mov	ax,CON(DPB_F_DEFAULT)
	jc	setdp2
	cmp	bl,CON(0x02)		; check if drive detects disk changes
	jne	setdp2
	or	ax,CON(DPB_F_DOOR)
setdp2:	mov	LOC(dpb_flags),ax	; set flags

; Thats it

	inc	BLOC(dpb_valid)		; increment valid flag
setdp9:	pop	di
	pop	si
	pop	es
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
; Print a string in DS:SI onto the console
;
; Changed registers: AL

prnstr:	push	si
	cld
prns1:	lodsb				; loop over all characters of
	or	al,al			; string
	jz	prns2
	push	bx
	mov	ah,CON(0x0E)		; print it
	mov	bl,CON(0x07)
	xor	bh,bh
	int	0x10
	pop	bx
	JMP(prns1)
prns2:	pop	si
	ret



#ifdef DRV_DEBUG
;====================================================================
;
; Print hexadecimal values (in AX or AL) or characters onto the console
;
; Changed registers: AX

prnwrd:	push	ax
	mov	al,ah
	call	prnbyt			; print the upper byte
	pop	ax
prnbyt:	push	ax
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
#endif



;====================================================================
;
; String and constants definitions


; Startup signature

sigmsg:	db	0x0D, 0x0A
	STRDECL('DOS Net Boot Image Loader ')
	STRDECL(VERSION)
	db	0x0D, 0x0A
	STRDECL(COPYRIGHT)
	db	0x0D, 0x0A
crlf:	db	0x0D, 0x0A
	db	0


; Magic numbers for boot record and bootp entry

bmagic:	dd	BOOT_MAGIC		; boot image magic number
vmagic:	STRDECL(VENDOR_MAGIC)		; vendor magic ID
	db	0			; end of vendor magic ID
pmagic:	db	BOOTP_MAGIC_RFC		; bootp magic ID for RFC 1048
	db	BOOTP_MAGIC_CMU		; bootp magic ID for CMU
	db	BOOTP_MAGIC_STA		; bootp magic ID for Stanford
	db	0


; Specifications for different types of disk drives. The order is:
; # dir entries, # sects per FAT, # sects per cluster, media ID

drvtab:	db	112, 2, 2, 0xfd		; 360kB disk
	db	224, 7, 1, 0xf9		; 1.2MB disk
	db	112, 3, 2, 0xf9		; 720kB disk
	db	224, 9, 1, 0xf0		; 1.44 MB disk

typtab:	db	DPB_T_360		; type values for drive parameter block
	db	DPB_T_1200
	db	DPB_T_720
	db	DPB_T_1440


; Error messages

recerr:	STRDECL('Error in load record data')
	db	0x0D, 0x0A
	db	0

bmerr:	STRDECL('Invalid boot header magic number')
	db	0x0D, 0x0A
	db	0

vmerr:	STRDECL('Invalid vendor magic ID')
	db	0x0D, 0x0A
	db	0

rderr:	STRDECL('Error while accessing ramdisk')
	db	0x0D, 0x0A
	db	0

dskerr:	STRDECL('Wrong ramdisk image')
	db	0x0D, 0x0A
	db	0

btperr:	STRDECL('BOOTP record invalid')
	db	0x0D, 0x0A
	db	0


; Debug messages

#ifdef ASM_DEBUG
debug1:	STRDECL('Making driver resident')
	db	0x0D, 0x0A
	db	0

debug2:
#ifdef ASM_FREEZE_AFTER_INIT
	STRDECL('Freezing;')
#else
	STRDECL('Calling boot block')
#endif
	db	0x0D, 0x0A
	db	0
#endif

#ifdef DRV_DEBUG
consz:	STRDECL('RAMDISK: reporting conventional memory size: ')
	db	0
extsz:	STRDECL('RAMDISK: reporting extended memory size: ')
	db	0
bytes:	STRDECL(' bytes')
	db	0x0D,0x0A
	db	0
#endif



;====================================================================
;
; Variable definitions

header:	dd	0			; pointer to boot header from boot rom
bootp:	dd	0			; pointer to bootp block from boot rom

resseg:	dw	0			; segment of resident section

oldDS:	dw	0			; old DS from boot rom
oldES:	dw	0			; old ES from boot rom
oldBP:	dw	0			; old BP from boot rom
oldSI:	dw	0			; old SI from boot rom
oldDI:	dw	0			; old DI from boot rom



;====================================================================
;
; Start of resident section. This will be placed at the end of the
; low 640kB RAM area.
;
;====================================================================
;

	ALIGN(16)			; has to be paragraph aligned

start_resident:				; indicate start of resident section


;====================================================================
;
; New interrupt 2Fh routine. This routine gets called by IO.SYS
; in order to determine the maximum amount of memory usable to
; DOS. This only works with DOS versions 5.0 and higher. The DOS
; function which gets installed into the interrupt 2Fh vector
; does not call the old vector (i.e. it does not daisy-chain),
; and therefore we can redirect 2Fh without a problem even when
; considering to remove the ram disk lateron.
;
; NOTE THAT THIS INTERRUPT HAS TO BE THE FIRST ROUTINE IN THE
; RESIDENT SECTION!
;
; Input:  AX     -  Magic ID
;         DX     -  segment following last usable byte
; Output: DX     -  new segment  following last usable byte
; Registers changed: DX

int2F:	JMP(int2F1)			; this has to be a relative jump
	nop

	STRDECL('RPL')			; magic ID string for DOS

int2F1:	cmp	ax,CON(0x4A06)		; check for magic ID
	jne	int2F9
	push	cx
	mov	dx,CON(start_resident)	; determine last usable segment
	mov	cl,CON(4)		; from segment and offset of
	shr	dx,cl			; the resident section
	pop	cx
	push	ax
	push	cs
	pop	ax
	add	dx,ax			; add offset to segment
	dec	dx
	pop	ax
	iret

int2F9:	SEGCS
	jmp	far [old2Fh]		; jump to old interrupt routine



;====================================================================
;
; New interrupt F8h routine. It can be used to retrieve several
; values from the resident driver.
; Input:  AX  -  function code
; Output: depends on function:
;
; Installation check (AX = 0x9C00)
;         AX     -  contains 0x009C
;
; Return ramdisk size and address (AX = 0x9C01)
;         BX:DX  -  address of ramdisk
;         CX     -  size of ramdisk in kb
;
; Return size and address of BOOTP block (AX = 0x9C02)
;         BX:DX  -  address of BOOTP block
;         CX     -  size of BOOTP block
;
; Return miscellaneous values for handling the ramdisk (AX = 0x9C03)
;         AX     -  XMS handle for ram disk
;         BX:DX  -  address of old interrupt vector table
;         CL     -  ramdisk id
;
; Remove ramdisk (AX = 0x9C04)
;         AL     -  non-zero if error
;
; Registers changed: depends on function

intF8:	cmp	ah,CON(0x9C)		; check for magic ID
	jne	intF89
	cmp	al,CON(01)		; check for function number
	jne	intF81
	SEGCS
	mov	bx,LOC(rdaddr+2)	; return ramdisk address
	SEGCS
	mov	dx,LOC(rdaddr+0)
	SEGCS
	mov	cx,LOC(rdsize)		; return ramdisk size
	iret

intF81:	cmp	al,CON(0x02)
	jne	intF82
	mov	bx,cs			; return address of BOOTP record
	mov	dx,CON(btpnew)
	SEGCS
	mov	cx,LOC(btplen)		; return BOOTP length
	iret

intF82:	cmp	al,CON(0x03)
	jne	intF83
	mov	bx,cs			; return address of old interrupt
	mov	dx,CON(oldints)		; vector table
	SEGCS
	mov	cl,LOC(drvid)		; return drive id
	SEGCS
	mov	ax,LOC(xmshdl)		; return XMS handle
	iret

intF83:	cmp	al,CON(0x04)
	jne	intF88
	call	rmrd			; remove ramdisk
	iret

intF88:	or	al,al
	jnz	intF89
	xchg	al,ah			; return installation check code
intF89:	iret



;====================================================================
;
; New interrupt 13h routine to handle disk accesses. It is different
; for simulating either a floppy drive or a hard disk. DOS provides
; a way for restoring this interrupt to its original value when we
; want to remove the ramdisk lateron.
; Input:  AH  -  function code
;         DL  -  driver number
; Output: carry flag set if error
; Registers changed: depends on function

int13:	sti				; we dont need interrupts disabled
	push	ax
	SEGCS
	mov	ax,LOC(xmsadr+0)
	SEGCS
	or	ax,LOC(xmsadr+2)	; check if XMS already initialized
	jnz	int13s
	mov	ax,CON(0x4300)		; check if XMS available
	int	0x2f
	cmp	al,CON(0x80)		; XMS not available
	jne	int13s
	push	bx
	push	es
	mov	ax,CON(0x4310)		; get XMS driver address
	int	0x2f
	SEGCS
	mov	LOC(xmsadr+0),bx	; save driver address
	SEGCS
	mov	LOC(xmsadr+2),es
	pop	es
	call	inixms			; initialize XMS
	pop	bx
int13s:	pop	ax

	SEGCS
	cmp	dl,LOC(drvid)		; check if its for us
	je	int132
	SEGCS
	cmp	BLOC(drvid),CON(0x80)	; check if we are to simulate a hard
	jae	int13h			; disk drive


; First comes the floppy drive redirector

	cmp	dl,CON(0x80)		; check if its for a hard disk
	jb	int133
	SEGCS
	test	BLOC(nohd),CON(0xff)	; check if hard disk accesses allowed
	jz	int131
	cmp	ah,CON(0x08)		; function 0x08 should not return error
	mov	ah,CON(0x80)		; return with error
	jne	int135
	xor	dl,dl			; indicate no hard disk present
	JMP(int13f)			; return without error

; Handle function 0x08 for disk drives other than the ramdisk.

int133:	cmp	ah,CON(0x08)
	jne	int131
	pushf
	SEGCS				; function 0x08 has to return the
	call	far [old13h]		; correct number of disk drives
	SEGCS
	mov	dl,LOC(drvnum)
int13f:	xor	ah,ah			; never return an error
	JMP(int136)

; Jump directly to the BIOS

int131:	SEGCS
	jmp	far [old13h]		; call the old interrupt routine

; Now handle all ramdisk functions. First check if the function number
; is correct.

int132:	cmp	ah,CON(0x18)
	jbe	int134
	mov	ah,CON(0x01)		; unknown command
int135:	stc
int136:	push	ds
	JMP(int13e)

; Determine the handlers address according to the function number in AH
; and jump to it.

int134:	push	ds
	push	cs
	pop	ds			; set data segment
	push	bx
	mov	bl,ah
	xor	bh,bh
	shl	bx,CON(1)		; compute pointer into routine table
	jmp	[bx+fntab]


; Now comes the hard disk drive redirector

int13h:	cmp	dl,CON(0x80)		; check if its for a floppy drive
	jb	int131

; Handle function 0x08 for hard disk drives other than the ramdisk.

	cmp	ah,CON(0x08)
	jne	int137
	dec	dl
	pushf
	SEGCS				; function 0x08 has to return the
	call	far [old13h]		; correct number of disk drives
	SEGCS
	mov	dl,LOC(drvnum)
	JMP(int13f)			; always return without error

; Handle function 0x15 for disk drives other than the ramdisk. This is
; the only function besides 0x08 which returns a value in DX and therefore
; has to have special handling.

int137:	push	dx
	dec	dl
	cmp	ah,CON(0x15)
	jne	int138
	pushf
	SEGCS
	call	far [old13h]		; call the BIOS for handling
	jc	int139
	cmp	ah,CON(0x03)		; DX is only used if AH = 0x03
	jne	int139
	add	sp,BCON(0x0002)		; remove DX from stack if the BIOS
	JMP(int136)			; returned a value in it

; Handle all other functions for drives other than the ramdisk. This will
; just call the original BIOS handler.

int138:	pushf
	SEGCS
	call	far [old13h]		; simply call the old int 13h routine
int139:	pop	dx
	JMP(int136)


; Save the return status into the BIOS data area and return to the caller
; while preserving the carry flag.

int13e:	push	es			; return from function handler
	push	bx			; this code is not allowed to change
	call	getsts			; any register or flag
	SEGES
	mov	[bx],ah			; set disk operation status
	pop	bx
	pop	es
intend:	pop	ds
	push	ax			; general exit point for interrupts
	pushf
	pop	ax
	push	bp
	mov	bp,sp
	mov	[bp+8],al		; put the flags onto the stack
	pop	bp
	pop	ax
	iret


; Function table

fntab:	dw	f1300			; function 00: reset disk system
	dw	f1301			; function 01: return last error
	dw	f1302			; function 02: read disk
	dw	f1303			; function 03: write disk
	dw	f1304			; function 04: verify disk
	dw	f1305			; function 05: format disk
	dw	f1306			; function 06: format track
	dw	f1307			; function 07: format disk
	dw	f1308			; function 08: get drive parameters
	dw	f1309			; function 09: intialize controller
	dw	f130A			; function 0A: read long sectors
	dw	f130B			; function 0B: write long sectors
	dw	f130C			; function 0C: seek for cylinder
	dw	f130D			; function 0D: disk reset
	dw	f130E			; function 0E: read sector buffer
	dw	f130F			; function 0F: write sector buffer
	dw	f1310			; function 10: check if drive ready
	dw	f1311			; function 11: recalibrate drive
	dw	f1312			; function 12: controller ram diagnostic
	dw	f1313			; function 13: drive diagnostic
	dw	f1314			; function 14: controller int diagnostic
	dw	f1315			; function 15: get disk type
	dw	f1316			; function 16: detect disk change
	dw	f1317			; function 17: set media type for format
	dw	f1318			; function 18: set media type for format

f13end:	JMP(int13e)



;====================================================================
;
; Function 00 - reset disk system
;

f1300:	test	WLOC(syscnf),CON(0x0001)	; check if we have physical floppy
	jz	f13001			; drives at all
	push	dx
	xor	dl,dl			; always reset the floppy system
	pushf
	call	far [old13h]		; call old disk interrupt
	pop	dx
	jc	f13002
f13001:	xor	ah,ah			; no error
f13002:	pop	bx
	JMP(f13end)



;====================================================================
;
; Function 01 - return last error status
;

f1301:	push	es
	call	getsts			; get offset to status byte
	SEGES
	mov	ah,[bx]			; get disk operation status
	pop	es
	pop	bx
	clc
	JMP(intend)



;====================================================================
;
; Function 02/03 - read/write from disk

f1302:
f1303:	pop	bx			; get old BX from stack
	push	dx
	push	ax
	call	cvtsec			; get linear sector number
	pop	ax
	jnc	f13021
	mov	ah,CON(0x04)		; error: sector not found
f13028:	pop	dx
	stc
	JMP(f13end)			; terminate

f13021:	push	cx
	push	bx
	push	ax
	mov	cl,al			; move number of sectors into CX
	xor	ch,ch
f13022:	jcxz	f13026
	cmp	dx,LOC(secnum)		; check if sector is still correct
	jb	f13023
	pop	ax
	mov	ah,CON(0x04)		; error: sector not found
f13027:	sub	al,cl			; compute number of sectors processed
	pop	bx
	pop	cx
	JMP(f13028)

f13023:	pop	ax
	push	ax
	xor	al,al
	cmp	ah,CON(0x02)		; check if read or write sector
	je	f13024
	inc	al
f13024:	call	rwsect			; actually handle request
	jnc	f13025
	pop	ax
	mov	ah,CON(0x20)		; error: disk controller error
	JMP(f13027)

f13025:	inc	dx
	dec	cx			; proceed with next sector
	add	bx,CON(SECT_SIZE)
	JMP(f13022)

f13026:	pop	ax
	pop	bx
	pop	cx
	pop	dx
	xor	ah,ah			; no error
f13e1:	JMP(f13end)



;====================================================================
;
; Function 08  -  get disk drive parameters

f1308:	pop	bx			; get old BX from stack
	mov	dl,LOC(drvnum)		; get number of disk drives
	mov	cl,LOC(secptk)		; get sectors per track
	mov	ch,LOC(cylnum)		; number of cylinders
	mov	dh,CON(1)		; number of heads - 1
	xor	bx,bx			; ramdisk drive type
	xor	ax,ax
	dec	ch
	JMP(f13e1)



;====================================================================
;
; Function 04, 05, 06, 07, 09, 0D, 10, 11, 12, 13, 14, 16  -  no operation

f1304:
f1305:
f1306:
f1307:
f1309:
f130D:
f1310:
f1311:
f1312:
f1313:
f1314:
f1316:	pop	bx			; get old BX from stack
	xor	ah,ah			; no error
	JMP(f13e1)



;====================================================================
;
; Function 0A, 0B, 0E, 0F, 17, 18  -  not implemented

f130A:
f130B:
f130E:
f130F:
f1317:
f1318:	pop	bx			; get old BX from stack
	mov	ah,CON(0x01)		; invalid opcode
	stc
	JMP(f13e1)



;====================================================================
;
; Function 0C  -  seek for cylinder

f130C:	pop	bx			; get old BX from stack
	push	dx
	push	ax
	call	cvtsec			; get linear sector number
	pop	ax
	mov	ah,CON(0x00)		; no error
	jnc	f130C1
	mov	ah,CON(0x04)		; error: sector not found
f130C1:	pop	dx
	JMP(f13e1)			; terminate



;====================================================================
;
; Function 15  -  get disk type

f1315:	pop	bx			; get old BX from stack
	mov	ah,CON(0x02)		; indicate floppy disk
	cmp	dl,CON(0x80)		; check if floppy disk type requested
	jb	f13159
	inc	ah			; indicate hard disk
	mov	dx,LOC(secnum)		; get number of sectors on disk
	xor	cx,cx
f13159:	clc
	JMP(f13e1)



;====================================================================
;
; Convert Cyl/Sec/Head notation into a linear sector number
; Input:  CH  -  cylinder number
;         CL  -  sector number
;         DH  -  head number
; Output: DX  -  linear sector number
;         carry flag set if invalid sector number
; Registers changed: AX,DX

cvtsec:	push	cx
	cmp	dh,CON(2)		; check if head number is correct
	jae	cvts8			; maximum number of heads is always 2
	mov	dl,dh
	xor	dh,dh			; move current head number into DX
	mov	al,ch
	mov	ch,cl
	mov	ah,cl			; compute track number into AX, the
	mov	cl,CON(6)		; upper two bits of CL are the high
	shr	ah,cl			; bits of the 10 bit cylinder number
	shl	ax,CON(1)		; compute track number from cylinders
	add	ax,dx
	mov	dl,LOC(secptk)
	xor	dh,dh
	mul	dx			; compute number of track starting
	or	dx,dx			; sector
	jnz	cvts8			; should not be more than 65535 sectors

	mov	dl,ch
	and	dl,CON(0x3F)		; move sector number into AX
	cmp	dl,LOC(secptk)		; check if sector number is correct
	ja	cvts8
	xor	dh,dh
	dec	dx			; sector numbers start with 1
	js	cvts8			; therefore sector 0 does not exist
	add	dx,ax			; compute final sector number
	jc	cvts8			; should never overflow
	cmp	dx,LOC(secnum)		; check if the sector is valid
	jae	cvts8
	clc				; no error
	JMP(cvts9)
cvts8:	stc				; return with error
cvts9:	pop	cx
	ret



;====================================================================
;
; Read/write a sector from the ram disk. This routine requires a
; sector to be 512 bytes long.
; Input:  AL     -  non-zero if write to ram disk
;         DX     -  logical sector number
;         ES:BX  -  pointer to destination buffer
; Output: carry flag set if error
; Registers changed: AX

rwsect:	push	cx
	push	dx
	mov	ch,al			; save direction indicator
	mov	dx,es
	mov	ax,dx
	mov	cl,CON(12)
	shr	ax,cl
	mov	cl,CON(4)
	shl	dx,cl			; compute linear buffer address
	add	dx,bx
	adc	ax,CON(0)
	or	ch,ch			; check direction of transfer
	jz	rwsec1
	mov	LOC(rd_srcb+0),dx	; set source address for write
	mov	LOC(rd_srcb+2),al
	JMP(rwsec2)
rwsec1:	mov	LOC(rd_dstb+0),dx	; set destination address for read
	mov	LOC(rd_dstb+2),al
rwsec2:	pop	dx

	push	dx
	mov	ax,dx
	mov	cl,CON(9)
	shl	dx,cl			; compute linear ramdisk address
	mov	cl,CON(7)		; from sector number
	shr	ax,cl
	add	dx,LOC(rdaddr+0)
	adc	ax,LOC(rdaddr+2)
	or	ch,ch			; check direction of transfer
	jz	rwsec3
	mov	LOC(rd_dstb+0),dx	; set destination address for write
	mov	LOC(rd_dstb+2),al
	JMP(rwsec4)
rwsec3:	mov	LOC(rd_srcb+0),dx	; set source address for read
	mov	LOC(rd_srcb+2),al
rwsec4:	push	es
	push	si
	mov	ax,cs
	mov	es,ax
	mov	si,CON(rd_gdt)
	mov	cx,CON(SECT_SIZE/2)	; copy 512 bytes, e.g. 256 words
	mov	ax,CON(0x8700)		; let the BIOS move the sector
	int	0x15
	pop	si
	pop	es
	pop	dx
	pop	cx
	ret



;====================================================================
;
; Return a pointer to the disk drive status byte. This routine should
; not change any flags!
; Input:  none
; Output: ES:BX  -  pointer to disk drive status byte
; Registers changed: BX, ES

getsts:	mov	bx,CON(BIOS_SEG)	; status byte is in BIOS data
	mov	es,bx			; segment
	SEGCS
	mov	bx,LOC(statof)
	ret



;====================================================================
;
; Initialize the XMS interface. This is necessary to prevent the
; ram disk from getting overwritten. The way this works is to
; first allocate all of the available XMS, then resize the memory
; block to end just above the ramdisk and lock it. Unfortunately
; we have to do it this complicated because there is no way of
; knowing how the XMS is going to allocate the available memory.
; Another problem is that at least HIMEM.SYS requires up to 256
; bytes of stack, and we cannot assume the caller of INT 13 to
; provide that much so we have to change stacks.
; Input:  none
; Output: none
; Registers changed: AX, BX

inixms:	call	setstk			; set new stack

; First check that the XMS version number is correct. To support all
; necessary functions it has to be version 2.0+.

	xor	ah,ah
	call	callxm			; get version number
	cmp	ah,CON(0x02)
	jb	inixm8			; wrong XMS version

; Determine how much memory we can allocate.

	mov	ah,CON(0x08)
	xor	bl,bl			; get amount of extended memory
	call	callxm
	or	bl,bl			; check for error
	jnz	inixm8
	mov	bx,LOC(rdsize)		; get size of ramdisk
	add	bx,BCON(65)		; care for a missing HMA
	cmp	bx,ax			; check if enough memory for ram disk
	ja	inixm8

; Grab all of the extended memory.

	push	bx
	mov	dx,ax			; grab largest block - which is whole
	mov	ah,CON(0x09)		; memory because there should be no
	call	callxm			; other process using XMS
	pop	bx
	or	ax,ax			; check for error
	jz	inixm8
	mov	LOC(xmshdl),dx		; save handle

; Now resize the memory block so that it will contain the ramdisk image.

	mov	ah,CON(0x0f)		; reallocate memory block
	call	callxm
	or	ax,ax			; check for error
	jnz	inixm1

inixm8:	mov	WLOC(xmshdl),CON(0)	; in case of error dont return handle
	JMP(inixm9)

; Now lock the memory block and check that the physical address of the
; memory block is correct.

inixm1:	mov	dx,LOC(xmshdl)
	mov	ah,CON(0x0c)		; lock memory block
	call	callxm
	add	bx,CON(0x03ff)
	adc	dx,BCON(0x0001)		; add 65kb - maximum difference
	sub	bx,LOC(rdaddr+0)	; check that ramdisk address is below
	sbb	dx,LOC(rdaddr+2)
	jc	inixm8

; Thats it. Restore all registers and swap the stack back to its
; original state.

inixm9:	call	rststk			; restore old stack
	ret



;====================================================================
;
; Call XMS driver.
; Input:  AH  -  function code
;         other registers depending on function code
; Output: depends on called function
; Registers changed: depends on called function

callxm:	push	ax
	push	bp
	push	ax
	mov	bp,sp
	mov	ax,[bp+6]
	mov	[bp+4],ax		; make far return address from
	mov	[bp+6],cs		; near call
	pop	ax
	pop	bp
	SEGCS
	push	WLOC(xmsadr+2) 		; push address of XMS driver
	SEGCS
	push	WLOC(xmsadr+0)
	retf				; call XMS driver



;====================================================================
;
; Set new stack
; Input:  none
; Output: none
; Registers changed: AX, BX, DS, SS, SP

setstk:	cli
	pop	bx			; get return address
	mov	ax,sp
	SEGCS
	mov	LOC(oldstk+0),ax
	SEGCS
	mov	LOC(oldstk+2),ss	; save old stack pointer
	mov	ax,cs
	mov	ss,ax
	mov	sp,CON(newtos - 2)	; change to new stack
	sti
	push	cx
	push	dx
	push	si			; save all registers
	push	di
	push	es
	push	ds
	mov	ax,cs			; set DS to current segment
	mov	ds,ax
	jmp	bx			; return to caller



;====================================================================
;
; Reset stack to old stack
; Input:  none
; Output: none
; Registers changed: all (reset to old state)


rststk:	pop	bx			; get return address
	pop	ds
	pop	es
	pop	di
	pop	si			; restore all registers
	pop	dx
	pop	cx
	cli
	SEGCS
	mov	ax,LOC(oldstk+0)	; restore old stack
	SEGCS
	mov	ss,LOC(oldstk+2)
	mov	sp,ax
	sti
	jmp	bx			; return to caller



;====================================================================
;
; Remove ramdisk from memory. This involves restoring all interrupt
; vectors, freeing all used memory and restoring the DOS drive para-
; meter table. Since we need to call the XMS drive, we have to switch
; stacks like with inixms.
; Input:  none
; Output: AL  -  non-zero if error
; Registers changed: AX

rmrd:	push	bx
	call	setstk			; set new stack
	mov	al,LOC(drvid)
	cmp	al,CON(0x80)		; can only restore floppy drives
	jb	rmrd1
rmrd8:	call	rststk			; return with error
	pop	bx
	mov	al,CON(0x01)
	ret

; First determine the address of the DOS disk parameter block for the
; ramdisk and check that the open count is zero, i.e. no open file on
; the device.

rmrd1:	push	ds
	mov	ax,CON(0x0803)
	int	0x2f			; get address of drive parameter
	mov	ax,ds			; table from DOS
	mov	es,ax
	pop	ds

rmrd2:	SEGES
	mov	al,[di+4]		; get physical unit number
	cmp	al,LOC(drvid)		; is it our drive?
	je	rmrd3
	cmp	di,CON(0xffff)		; error if we couldnt find the DPB
	je	rmrd8			; for the ramdisk
	SEGES
	les	di,[di]			; get pointer to next entry
	JMP(rmrd2)

rmrd3:	mov	LOC(dpb_addr+0),di
	mov	LOC(dpb_addr+2),es
	SEGES
	mov	ax,[di+0x20]		; get device open count
	or	ax,ax
	jnz	rmrd8

; Next restore the interrupt vectors. Int 13h is special as it is
; redirected by DOS. However, DOS provides a function to restore
; that interrupt. Interrupt 2Fh doesnt have to get restored because
; DOS does never call the old interrupt again.

	xor	ax,ax
	mov	es,ax
	mov	ax,cs
	SEGES				; first check that nobody redirected
	cmp	ax,LOC(IF8_INT+2)	; our own interrupts. In that case
	jne	rmrd8			; there is no chance of removing the
	SEGES				; ramdisk.
	mov	ax,LOC(IF8_INT+0)
	cmp	ax,CON(intF8)
	jne	rmrd8
	mov	si,CON(if1sig)
	mov	di,CON(IF1_INT)
	mov	cx,CON(4)		; interrupt F1h contains a signature
	repz				; and no vector
	cmpsb
	jnz	rmrd8

	push	ds
	les	bx,LOC(old13h)		; get old interrupt vector 13h
	mov	dx,bx
	mov	ax,es			; save it into DS:DX and ES:BX
	mov	ds,ax
	mov	ah,CON(0x13)
	int	0x2f			; call DOS to restore vector
	mov	ax,ds
	mov	cx,cs
	cmp	ax,cx			; check that its indeed our interrupt
	jne	rmrd4			; which we are replacing
	mov	ax,es
	cmp	ax,cx
	jne	rmrd4
	cmp	bx,CON(int13)
	jne	rmrd4
	cmp	dx,CON(int13)
	je	rmrd5

rmrd4:	mov	ah,CON(0x13)
	int	0x2f			; restore old interrupt
	pop	ds			; someone redirected the interrupt
#ifdef	USE_AS86
	jmp	near rmrd8		; already, cant restore
#endif
#ifdef	USE_NASM
	jmp	rmrd8			; already, cant restore
#endif

rmrd5:	pop	ds			; restore the other interrupts
	cli
	xor	ax,ax
	mov	es,ax
	mov	ax,LOC(oldF8h+0)
	SEGES
	mov	LOC(IF8_INT+0),ax
	mov	ax,LOC(oldF8h+2)
	SEGES
	mov	LOC(IF8_INT+2),ax
	sti

; OK, we can now setup the DOS drive parameter table to contain the
; correct values for the physical floppy drive. If we couldnt create
; a valid parameter table entry for this drive, simply mark the DOS
; entry as invalid. This will cause "Not Ready" errors in DOS. This
; doesnt work with DR-DOS 5.0!

	les	di,LOC(dpb_addr)	; get address of DPB
	SEGES
#ifdef	USE_AS86
	or	[di+0x1f],BCON(80)	; mark drive as invalid
#endif
#ifdef	USE_NASM
	or	word [di+0x1f],BCON(80)	; mark drive as invalid
#endif
	test	BLOC(dpb_valid),CON(0xff)	; check if DPB valid
	jz	rmrd6
	cld				; got correct table entry
	mov	cx,CON(dpb_end - dpb)
	mov	si,CON(dpb)
	add	di,BCON(4)
	rep
	movsb				; simply copy the DPB

; Next remove the ramdisk image from extended memory using the XMS driver.

rmrd6:	mov	dx,LOC(xmshdl)
	or	dx,dx			; only free memory if we really
	jz	rmrd7			; assigned it with XMS
	push	dx
	mov	ah,CON(0x0d)
	call	callxm			; unlock memory block
	pop	dx
	or	ax,ax			; dont free block if error
	jz	rmrd7
	mov	ah,CON(0x0a)
	call	callxm			; free memory block

; Finally we can remove the memory for the ramdisk driver. We only
; reset the owner field of the memory control block to 0 to indicate
; it as free.

rmrd7:	mov	dx,CON(start_resident)	; determine last usable segment
	mov	cl,CON(4)		; from segment and offset of
	shr	dx,cl			; the resident section
	mov	ax,cs
	add	dx,ax			; add offset to segment
	sub	dx,BCON(2)
	mov	es,dx
	mov	di,CON(1)
	xor	ax,ax			; set owner field to 0
	stosw
	add	di,BCON(5)
	mov	cx,CON(4)		; clear owner name
	rep
	stosw

; Thats it. Return to caller.

rmrd9:	call	rststk			; restore old stack
	pop	bx
	xor	al,al			; return without error
	ret



;====================================================================
;
; Variables for the resident section

		ALIGN(2)

oldints:
old13h:		dd	0		; old interrupt 13h vector
old2Fh:		dd	0		; old interrupt 2Fh vector
oldF1h:		dd	0		; old interrupt F1h vector
oldF8h:		dd	0		; old interrupt F8h vector


; Disk parameters for ram disk

statof:		dw	BIOS_FDSTAT	; offset to BIOS disk status byte
rdaddr:		dd	0		; base address of ram disk
rdsize:		dw	0		; size of ram disk in kb
cylnum:		dw	80		; number of cylinders
secnum:		dw	2400		; number of sectors on disk
secptk:		dw	15		; number of sectors per track
drvnum:		db	1		; number of disk drives
drvid:		db	0		; ram disk drive id
nohd:		db	0		; no-hard-disk flag
syscnf:		dw	0		; system configuration from BIOS

		ALIGN(2)


; Variables used to access the XMS interface

xmshdl:		dw	0		; XMS handle for ram disk
xmsadr:		dd	0		; address of XMS driver interface


; Variables used to redirect the stack

oldstk:		dd	0		; old stack pointer
newstk:		SPACE(512)		; new stack for calling XMS driver
newtos:					; new top of stack


; Signature to put into interrupt vector F1h

if1sig:		STRDECL('NetB')


; Descriptor table to access ram disk using the BIOS

rd_gdt:		dw	0,0,0,0
		dw	0,0,0,0
rd_src:		dw	0xffff		; length
rd_srcb:	db	0,0,0		; base
		db	0x93		; typebyte
		dw	0		; limit16,base24 =0
rd_dst:		dw	0xffff		; length
rd_dstb:	db	0,0,0		; base
		db	0x93		; typebyte
		dw	0		; limit16,base24 =0
		dw	0,0,0,0		; BIOS CS
		dw	0,0,0,0		; BIOS DS


; DOS disk parameter block. It contains the definitions for the
; floppy disk drive which is redirected by the ramdisk, and used
; for removing the ramdisk drive. Note that this DPB is only
; valid for DOS 4.0 and higher.

dpb_addr:	dd	0		; address of DPB in DOS data area
dpb_valid:	db	0		; non-zero if DPB is valid

dpb:
dpb_phys:	db	0		; BIOS ID of physical drive
dpb_log:	db	0		; logical DOS drive ID

dpb_bpb_low:	dw	512		; BIOS param block for lowest capacity
		db	0xff
		dw	1
		db	2
		dw	64
		dw	360
		db	0x00
		dw	2
		dw	9
		dw	1
		dd	0
		dd	0

dpb_fat:	db	0		; flag indicating 16-bit FAT
dpb_open:	dw	0		; device open count
dpb_type:	db	0x01		; device type
dpb_flags:	dw	DPB_F_DEFAULT	; flags describing drive
dpb_cyls:	dw	80		; number of cylinders

dpb_bpb_cur:	dw	512		; BIOS parameter block for current
		db	1
		dw	1
		db	2
		dw	224
		dw	2400
		db	0xf9
		dw	7
		dw	15
		dw	2
		dd	0
		dd	0

dpb_rsvd:	db	0, 0, 0, 0, 0, 0
dpb_ltrack:	db	0xff		; last accessed track
dpb_lacc:	dd	0xffffffff	; time of last disk access
dpb_volname:	STRDECL('NO NAME    ')	; volume name
		db	0
dpb_sernum:	dd	0		; volume serial number
dpb_fsname:	STRDECL('FAT12   ')	; file system name
		db	0
dpb_end:


; Copy of bootp block from bootrom. This has to be last in the data area!

btplen:		dw	0		; length of bootp block
btpnew:					; bootp block has to be at the very end

