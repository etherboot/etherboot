; At entry, the processor is in 16 bit real mode and the code is being
; executed from an address it was not linked to. Code must be pic and
; 32 bit sensitive until things are fixed up.

#ifdef	ZLOADER
; LZHuf (LZSS) Decompressing boot loader for ROM images
;
; this code is based on the work of Haruyasu Yoshizaki and Haruhiko Okumura
; who implemented the original compressor and decompressor in C code
;
; Copyright 1997 by M. Gutschke <gutschk@math.uni-muenster.de>
;
; Compression pays off, as soon as the uncompressed image is bigger than
; about 1.5kB. This assumes an average compressibility of about 60%.
#endif

/* This provides for execution at 0x88000 to avoid disk-on-chip drivers */
#undef	NOT_AT_0x9xxxx
#if	(RELOC & 0xF0000) != 0x90000
#define	NOT_AT_0x9xxxx
#endif

/* If the Makefile does not provide configuration options, then set
 * reasonable default values. */

#ifdef	MOVEROM
#undef	MOVEROM
/* Check if we are running from somewhere other than 0x9xxxx */
#ifdef	NOT_AT_0x9xxxx
#define MOVEROM		0x70000
#else
#define MOVEROM		0x80000
#endif
#endif

/* Scratch space has to be at least 10kB; but because of the relocation
 * address at 0x98000 we cannot use more then 32kB for uncompressed data
 * anyways, so lets give all the rest of the segment to the scratch area. */

#ifndef	SCRATCH
#define SCRATCH		0x8000
#endif


/* We need some unique magic ID, if we defer startup thru the INT19h
 * handler. This way, we can check if we have already been installed. */

#ifndef	MAGIC
#define MAGIC		0xE44C
#endif

/* Hook into INT19h handler */

#define INT19VEC	0x64
#define SCRATCHVEC	0x300

/* Offsets of words containing ROM's CS and size (in 512 byte blocks)
 * from start of floppy boot block at 0x7C00 */

#define	FLOPPY_SEGMENT	0x7C0
#define	ROM_SEGMENT	506
#define	ROM_LENGTH	508

/* Do not change these values unless you really know what you are doing;
 * the pre-computed lookup tables rely on the buffer size being 4kB or
 * smaller. The buffer size must be a power of two. The lookahead size has
 * to fit into 6 bits. If you change any of these numbers, you will also
 * have to adjust the compressor accordingly. */

#define BUFSZ		4096
#define LOOKAHEAD	60
#define THRESHOLD	2
#define NCHAR		(256+LOOKAHEAD-THRESHOLD)
#define TABLESZ		(NCHAR+NCHAR-1)
#define ROOT		(TABLESZ-1)

/* Adjust to AS86 or NASM syntax. */

#ifdef	USE_AS86
#define	CON(x)	*x
#define	LOC(x)	x
#define STRDECL(x)	.ascii	x
#define	JMP	jmp
#define JMPL	br
#define SEG_CS	seg	cs
	.text
#endif
#ifdef	USE_NASM
#define	CON(x)	x
#define	LOC(x)	[x]
#define STRDECL(x)	db	x
#define	JMP	jmp short
#define JMPL	jmp
#define SEG_CS	cs
	section	.text
#endif

_start:
	dw	0xAA55			; BIOS extension signature
size:	db	0			; number of 512 byte blocks
					; = number of 256 word blocks
					; filled in by makerom program
	JMP	over			; skip over checksum
	db	0			; checksum
#ifndef	PCI_PNP_HEADER
	JMP	blockmove		; alternate entry point +6
					; used by floppyload and comload
#else	/* PCI_PNP_HEADER */
	JMPL	blockmove		; alternate entry point +6
					; used by floppyload and comload
	STRDECL('Etherboot')		; Might as well put something here
#ifdef	USE_AS86
	rmb	0x18-(*-_start)
#endif
#ifdef	USE_NASM
	times	0x18-($-_start) db 0
#endif
	dw	PCI			; offset to pci data structure
	dw	PnP			; offset to PnP expansion header

PCI:
	STRDECL('PCIR')			; signature
	dw	0x8086			; vendor ID, filled in by makerom
	dw	0x1229			; device ID, filled in by makerom
	dw	0x0000			; pointer to vital product data
	dw	0x0018			; PCI data structure length
	db	0			; PCI data structure revision
	db	0			; Class code byte 1
	db	0			; Class code byte 2
	db	0x02			; Class code byte 3 (from hexdumping
					; Intel bootrom image)
	dw	0x0000			; Image length same as offset 02h
	dw	0x0001			; revision level of code /data
	db	0			; code type
	db	0x80			; indicator (from hexdumping
					; Intel bootrom image)
	dw	0x0000			; reserved

PnP:
	STRDECL('$PnP')			; signature
	db	0x01			; structure revision
	db	0x02			; length (in 16 byte increments)
	dw	0x0000			; offset of next header
	db	0			; Reserved
	db	0			; checksum filled by makerom
	dd	0x00000000		; Device identifier
	dw	0x0000			; pointer to manufacturer str
	dw	0x0000			; pointer to product name
	db	0			; device type code byte 1
	db	0			; device type code byte 2
	db	2			; device type code byte 3 (from
					; hexdumping Intel bootrom image)
	db	0x14			; device indictors (from
					; hexdumping Intel bootrom image)
	dw	0x0000			; boot connection vector
	dw	0x0000			; disconnect vector
	dw	over			; bootstrap entry vector
	dw	0x0000			; reserved
	dw	0x0000			; static resource information vector
#endif
over:
#ifndef	NOINT19H
	push	ax
	push	ds
	xor	ax,ax
	mov	ds,ax			; access first 64kB segment
	mov	ax,LOC(SCRATCHVEC+4)	; check if already installed
	cmp	ax,CON(MAGIC)		; check magic word
	jz	installed
	mov	ax,LOC(INT19VEC)	; hook into INT19h
	mov	LOC(SCRATCHVEC),ax
	mov	ax,LOC(INT19VEC+2)
	mov	LOC(SCRATCHVEC+2),ax
	mov	ax,CON(start19h)
	mov	LOC(INT19VEC),ax
	mov	ax,cs
	mov	LOC(INT19VEC+2),ax
	mov	ax,CON(MAGIC)		; set magic word
	mov	LOC(SCRATCHVEC+4),ax
installed:
	pop	ds
	pop	ax
	mov	ax,0x20
	retf
start19h:				; clobber magic id, so that we will
	xor	ax,ax			; not inadvertendly end up in an
	mov	ds,ax			; endless loop
	mov	LOC(SCRATCHVEC+4),ax
	mov	ax,LOC(SCRATCHVEC+2)	; restore original INT19h handler
	mov	LOC(INT19VEC+2),ax
	mov	ax,LOC(SCRATCHVEC)
	mov	LOC(INT19VEC),ax
#endif	/* NOINT19H */
	/* fall thru */
blockmove:
; The following copy is a bit convoluted to save source code, so here a few
; lines on the why of all this. The standard (non-compressed) loader needs to
; copy its payload to the RELOC:0 area in any case. The compressed loader
; offers an option to move the whole ROM contents to RAM (at MOVEROM:0) before
; excuting it. This is just a speed improvement on systems where the ROM area
; is not cached. The compressed loader (no matter if it executes from RAM or
; ROM) decompresses its payload to RELOC:0.
	xor	cx,cx
	SEG_CS				; override, source = cs
	mov	ch,LOC(size-_start)
#if	!defined(ZLOADER) || defined(MOVEROM)
	cld
	mov	dx,cx			; save length (no stack yet!)
#ifdef	ZLOADER /* copy full ROM contents to RAM to improve execution speed */
	mov	ax,CON(MOVEROM>>4)
	xor	si,si			; si = 0
#else		/* copy only payload to RELOC:0 */
	mov	ax,CON(RELOC>>4)
	mov	si,CON(payload-_start)	; offset of code image
	mov	bx,si
	shr	bx,1
	sub	cx,bx			; calculate legth of payload
#endif
	mov	es,ax
	xor	di,di			; di = 0
	SEG_CS				; override, source = cs
	rep
	movsw
	mov	cx,dx			; reload length

#ifdef	ZLOADER
	jmp	MOVEROM>>4:moved	; reload cs:ip to execute from RAM
moved:
#endif
#endif

; Save ROMs CS and length in floppy boot block
	mov	ax,CON(FLOPPY_SEGMENT)
	mov	ds,ax
	mov	ax,cs
	mov	LOC(ROM_SEGMENT),ax
	mov	LOC(ROM_LENGTH),cx

; Change segment registers and stack
	mov	bx,CON(RELOC>>4)	; new ss
#ifdef	ZLOADER
	mov	ds,ax			; new ds (loaded above from cs)
	mov	ax,CON((RELOC-SCRATCH)>>4); first 32kB -> scratch space
	mov	es,ax			; new es (for data copying in init)
#else
	mov	ds,bx			; new ds
#endif
/* if we are not at 0x9xxxx, then don't use INT12 to set stack */
#ifdef	NOT_AT_0x9xxxx
	mov	ax,CON(0x0000)		; use "maximum" stack pointer
#else
	int	0x12			; get conventional memory size in KB
	mov	cl,CON(6)
	shl	ax,cl			; 8086 cannot do shl ax,6!
	sub	ax,bx
#ifndef DONT_CHECK_STACKOFFSET
	test	ax,CON(0xf000)		; would sp wrap the segment limit?
	jz	stackok
rev:	js	rev			; segment is too short, just crash
	mov	ax,CON(0x0000)		; use "maximum" stack pointer
; Note this only works because the stack contains at least a return address
; while in protected mode, otherwise the different wraparound behaviour in
; real and protected mode would result in random crashes.
stackok:
#endif
#endif	/* NOT_AT_0x9xxxx */
	mov	cl,CON(4)
	shl	ax,cl			; new sp
	mov	ss,bx
	mov	sp,ax

#ifdef	ZLOADER
;----------------------------------------------------------------------------

; INIT -- initializes all data structures
; ====

init:	cld
	mov	si,CON(dcodrle-_start)	; uncompress run length encoded
	mov	di,CON(dcode)		; lookup table for codes
	mov	dl,CON(6)
	mov	dh,CON(0x20)
	xor	bh,bh
init0:	lodsb
	mov	bl,al
init1:	mov	cl,dh
	xor	ch,ch
	mov	al,bh
	rep
	stosb
	inc	bh
	dec	bl
	jnz	init1
	shr	dh,1
	dec	dl
	jnz	init0
	mov	bl,CON(1)		; uncompress run length encoded
	mov	bh,CON(6)		; lookup table for code lengths
init2:	lodsb
	mov	cl,al
	xor	ch,ch
	mov	al,bl
	rep
	stosb
	inc	bl
	dec	bh
	jnz	init2
	mov	ax,es			; we no longer have to access static
	mov	ds,ax			; data, so set segment accordingly
	mov	cx,CON(NCHAR)		; set all frequencies of leaf nodes
	mov	ax,CON(1)		; to one
	rep
	stosw
	mov	si,CON(freq)
	mov	cx,CON(ROOT+1-NCHAR)
init3:	lodsw				; update frequencies of non-leaf nodes
	mov	bx,ax
	lodsw
	add	ax,bx
	stosw
	loop	init3
	mov	ax,CON(0xFFFF)
	stosw				; sentinel with infinite frequency
	mov	cx,CON(NCHAR)
	mov	ax,CON(TABLESZ)
init4:	stosw				; update "son" pointers for leaf nodes
	inc	ax
	loop	init4
	mov	cx,CON(ROOT+1-NCHAR)
	xor	ax,ax
init5:	stosw				; update "son" ptrs for non-leaf nodes
	add	ax,CON(2)
	loop	init5
	mov	cx,CON(ROOT+1-NCHAR)
	mov	ax,CON(NCHAR)
init6:	stosw				; update "parent" ptrs for non-leaf nd.
	stosw
	inc	ax
	loop	init6
	mov	cx,CON(NCHAR)
	xor	ax,ax
	stosw				; root node has no parent
init7:	stosw				; update "parent" ptrs for leaf nodes
	inc	ax
	loop	init7
	xor	ax,ax
	stosb				; clear getlen
	stosw				; clear getbuf
	mov	al,CON(0x20)		; fill text buffer with spaces
	mov	di,CON(spaces)
	mov	cx,CON(BUFSZ-LOOKAHEAD)
	rep
	stosb
	/* fall thru */


;----------------------------------------------------------------------------

; MAIN -- reads compressed codes and writes decompressed data
; ====

	mov	si,CON(payload-_start); get length of compressed data stream
	mov	di,CON(uncompressed)
	SEG_CS
	lodsw
	mov	cx,ax
	lodsw				; cannot do more than 64k anyways
main1:	push	cx
	call	dcdchr			; decode one code symbol
	or	ah,ah			; test if 8bit character
	jnz	main2
	stosb				; store verbatim
	pop	cx
	loop	main1			; proceed with next compressed code
	JMP	done			; until end of input is detected
main2:	push	ax
	call	dcdpos			; compute position in output buffer
	mov	ax,si
	sub	bx,di
	not	bx
	mov	si,bx			; si := di - dcdpos() - 1
	pop	cx
	sub	cx,CON(255-THRESHOLD)	; compute length of code sequence
	mov	dx,cx
	rep
	movsb
	mov	si,ax
	pop	cx
	sub	cx,dx			; check end of input condition
	jnz	main1			; proceed with next compressed code
done:
	mov	ax,CON(RELOC>>4)	; set ds then call etherboot
	mov	ds,ax
	mov	es,ax
#endif	/* ZLOADER */

#ifdef	ENTRYPOINT
	call	ENTRYPOINT
#else
	call	RELOC>>4:0
#endif
	int	0x19

#ifdef	ZLOADER
;----------------------------------------------------------------------------

; GETBIT -- gets one bit pointed to by DS:SI
; ======
;
; changes: AX,CX,DL

getbit:	mov	cl,CON(8)
	mov	dl,LOC(getlen)		; compute number of bits required
	sub	cl,dl			; to fill read buffer
	jae	getbit1
	mov	ax,LOC(getbuf)		; there is still enough read ahead data
	JMP	getbit2
getbit1:SEG_CS
	lodsb				; get next byte from input stream
	xor	ah,ah
	shl	ax,cl			; shift, so that it will fit into
	mov	cx,LOC(getbuf)		; read ahead buffer
	or	ax,cx
	add	dl,CON(8)		; update number of bits in buffer
getbit2:mov	cx,ax
	shl	cx,1			; extract one bit from buffer
	mov	LOC(getbuf),cx
	dec	dl
	mov	LOC(getlen),dl		; and update number of bits
	shl	ax,1			; return in carry flag
	ret


;----------------------------------------------------------------------------

; DCDPOS -- decodes position in textbuffer as pointed to by DS:SI, result in BX
; ======
;
; changes: AX,BX,CX,DX

dcdpos:	mov	bx,CON(0x0800)
dcdpos1:shl	bl,1			; read one byte
	call	getbit
	jnc	dcdpos2
	inc	bl
dcdpos2:dec	bh
	jnz	dcdpos1
	mov	dh,bl			; read length of code from table
	xor	bh,bh
	mov	cl,[bx+dlen]
	xor	ch,ch
	mov	bl,[bx+dcode]		; get top six bits from table
	shl	bx,1
	shl	bx,1
	shl	bx,1
	shl	bx,1
	shl	bx,1
	shl	bx,1
dcdpos3:push	cx			; read the rest from the input stream
	shl	dh,1
	call	getbit
	jnc	dcdpos4
	inc	dh
dcdpos4:pop	cx
	loop	dcdpos3
	and	dh,CON(0x3f)		; combine upper and lower half of code
	or	bl,dh
	ret


;----------------------------------------------------------------------------

; DCDCHR -- decodes one compressed character pointed to by DS:SI
; ======
;
; changes: AX,BX,CX,DX

dcdchr:	mov	bx,CON(ROOT)		; start at root entry
	shl	bx,1
	mov	bx,[bx+son]
dcdchr1:call	getbit			; get a single bit
	jnc	dcdchr2
	inc	bx			; travel left or right
dcdchr2:shl	bx,1
	mov	bx,[bx+son]
	cmp	bx,CON(TABLESZ)		; until we come to a leaf node
	jb	dcdchr1
	mov	ax,bx
	sub	ax,CON(TABLESZ)
	/* fall thru */

;


; UPDATE -- updates huffman tree after incrementing frequency for code in BX
; ======
;
; changes: BX,CX,DX

update:	; we do not check whether the frequency count has overrun.
	; this will cause problems for large files, but be should be fine
	; as long as the compressed size does not exceed 32kB and we
	; cannot do more than this anyways, because we load into the
	; upper 32kB of conventional memory
	push	si
	push	ax
	shl	bx,1
	mov	bx,[bx+parent]
update1:shl	bx,1
	mov	dx,[bx+freq]
	inc	dx			; increment frequency count by one
	mov	[bx+freq],dx
	mov	si,bx
	add	si,CON(freq+2)
	lodsw				; check if nodes need reordering
	cmp	dx,ax
	jbe	update5
update2:lodsw
	cmp	ax,dx
	jb	update2
	mov	cx,[si-4]
	mov	[bx+freq],cx		; swap frequency of entries
	mov	[si-4],dx
	mov	ax,si			; compute index of new entry
	sub	ax,CON(freq+4)
	mov	dx,ax
	shr	ax,1
	mov	cx,[bx+son]		; get son of old entry
	mov	si,cx
	add	si,si
	mov	[si+parent],ax		; and update the ptr to new parent
	cmp	cx,CON(TABLESZ)
	jae	update3			; do this for both branches
	mov	[si+parent+2],ax	; if not a leaf node
update3:mov	si,dx
	mov	dx,[si+son]		; get son of new entry
	mov	[si+son],cx		; update its contents
	mov	si,dx
	add	si,si
	mov	cx,bx
	shr	cx,1
	mov	[si+parent],cx		; and update the ptr to new paren
	cmp	dx,CON(TABLESZ)
	jae	update4			; do this for both branches
	mov	[si+parent+2],cx	; if not a leaf node
update4:mov	[bx+son],dx		; update son of old entry
	mov	bx,ax			; continue with new entry
	shl	bx,1
update5:mov	bx,[bx+parent]		; continue with parent
	or	bx,bx
	jnz	update1			; until we found the root entry
	pop	ax
	pop	si
	ret


;----------------------------------------------------------------------------

; constant data. this part of the program resides in ROM and cannot be
; changed

; run length encoded tables will be uncompressed into the bss segment
; take care with any symbols here for .com files to add 0x100 to address

dcodrle:db	0x01,0x03,0x08,0x0C,0x18,0x10
dlenrle:db	0x20,0x30,0x40,0x30,0x30,0x10


;----------------------------------------------------------------------------

; variable data segment (bss)
; this segment will always be found at 0x90000 (i.e. at RELOC - SCRATCH)

; do not change the order or the sizes of any of the following tables
; the initialization code makes assumptions on the exact layout of the
; data structures...


; lookup table for index into buffer of recently output characters

dcode	equ	0


; lookup table for length of code sequence from buffer of recent characters

dlen	equ	dcode+256


; table with frequency counts for all codes
freq	equ	dlen+256


; pointer to child nodes
son	equ	freq+2*(TABLESZ+1)


; the first part of this table contains all the codes	(0..TABLESZ-1)
; the second part contains all leaf nodes		(TABLESZ..)
parent	equ	son+2*(TABLESZ)


; temporary storage for extracting bits from compressed data stream
getlen	equ	parent+2*(TABLESZ+NCHAR)
getbuf	equ	getlen+1

; the initial buffer has to be filled with spaces (size: BUFSZ+LOOKAHEAD)
spaces	equ	SCRATCH-BUFSZ+LOOKAHEAD


; uncompressed data will be written to address 0x98000

uncompressed	equ	SCRATCH
#endif	/* ZLOADER */

; Force 4 byte alignment
#ifdef	USE_AS86
	if	((*-_start)&3) != 0
	rmb	3-((*-_start)&3)
	db	0
	endif
#endif
#ifdef	USE_NASM
	%if	(($-_start)&3) != 0
	times	3-(($-_start)&3) db 0
	db	0
	%endif
#endif
payload:
; the (compressed) code will be attached here
