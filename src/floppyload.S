; NOTE: this boot sector contains instructions that need at least an 80186.
; Yes, as86 has a bug somewhere in the valid instruction set checks.
;
; SYS_SIZE is the number of clicks (16 bytes) to be loaded.
;
SYSSIZE equ 2048	; 2048 * 16 bytes = 32kB maximum size of .ROM file

;	floppyload.S		Copyright (C) 1991, 1992 Linus Torvalds
;	modified by Drew Eckhardt
;	modified by Bruce Evans (bde)
;
; floppyload.S is loaded at 0x0000:0x7c00 by the bios-startup routines.
;
; It then loads the system at SYSSEG<<4, using BIOS interrupts.
;
; The loader has been made as simple as possible, and continuous read errors
; will result in a unbreakable loop. Reboot by hand. It loads pretty fast by
; getting whole tracks at a time whenever possible.

#ifdef	USE_AS86
#define	CON(x)	*x
#define	BCON(x)	*x
#define	LOC(x)	x
#define	WLOC(x)	x
#define	STRDECL(x)	.ascii	x
#define	JMP	jmp
#define	SEG_ES	seg	es
	.text
#endif
#ifdef	USE_NASM
#define	CON(x)	x
#define	BCON(x)	byte x
#define	LOC(x)	[x]
#define	WLOC(x)	word [x]
#define	STRDECL(x)	db	x
#define	JMP	jmp short
#define	SEG_ES	es
	section	.text
#endif
	org	0

BOOTSEG	equ	0x07C0			; original address of boot-sector

SYSSEG	equ	0x1000			; system loaded at SYSSEG<<4

	jmp	BOOTSEG:go		; reload cs:ip to match relocation addr
go:
	mov	di,CON(0x2000-12)	; 0x2000 is arbitrary value >= length
					; of bootsect + room for stack + 12 for
					; saved disk parm block

	mov	ax,CON(BOOTSEG)
	mov	ds,ax
	mov	es,ax
	mov	ss,ax			; put stack at BOOTSEG:0x4000-12.
	mov	sp,di

; Many BIOS''s default disk parameter tables will not recognize multi-sector
; reads beyond the maximum sector number specified in the default diskette
; parameter tables - this may mean 7 sectors in some cases.
;
; Since single sector reads are slow and out of the question, we must take care
; of this by creating new parameter tables (for the first disk) in RAM.  We
; will set the maximum sector count to 36 - the most we will encounter on an
; ED 2.88.  High doesn''t hurt.  Low does.
;
; Segments are as follows: ds=es=ss=cs - BOOTSEG

	xor	cx,cx
	mov	es,cx			; access segment 0
	mov	bx,CON(0x78)
	push	ds			; save ds
; 0:bx is parameter table address
	SEG_ES
	lds	si,[bx]			; loads ds and si

	mov	es,ax			; ax is BOOTSECT (loaded above)
	mov	cl,CON(6)		; copy 12 bytes
	cld
	push	di			; keep a copy for later
	rep
	movsw				; ds:si is source, es:di is dest
	pop	di

	SEG_ES
	mov	byte [di+4],CON(36)	; patch sector count

	mov	ds,cx			; access segment 0
	xchg	[bx],di
	mov	si,es
	xchg	[bx+2],si
	pop	ds			; restore ds
	mov	LOC(dpoff),di		; save old parameters
	mov	LOC(dpseg),si		; to restore just before finishing
	push	ds
	pop	es			; reload es

; Note that es is already set up.  Also cx is 0 from rep movsw above.

	xor	ah,ah			; reset FDC
	xor	dl,dl
	int	0x13

; Get disk drive parameters, specifically number of sectors/track.

; It seems that there is no BIOS call to get the number of sectors.  Guess
; 36 sectors if sector 36 can be read, 18 sectors if sector 18 can be read,
; 15 if sector 15 can be read.  Otherwise guess 9.

	mov	si,CON(disksizes)	; table of sizes to try

probe_loop:
	lodsb
	cbw				; extend to word
	mov	LOC(sectors),ax
	cmp	si,CON(disksizes+4)
	jae	got_sectors		; if all else fails, try 9
	xchg	ax,cx			; cx = track and sector
	xor	dx,dx			; drive 0, head 0
	mov	bx,CON(0x0200)		; address after boot sector
					;   (512 bytes from origin, es = cs)
	mov	ax,CON(0x0201)		; service 2, 1 sector
	int	0x13
	jc	probe_loop		; try next value

got_sectors:
	mov	cx,CON(msg1end-msg1)
	mov	si,CON(msg1)
	call	print_str

; ok, we''ve written the Loading... message, now we want to load the system

	push	es			; = ds
	mov	ax,CON(SYSSEG)
	mov	es,ax			; segment of SYSSEG<<4
	push	es
	call	read_it

; This turns off the floppy drive motor, so that we enter the kernel in a
; known state, and don''t have to worry about it later.
	mov	dx,CON(0x3f2)
	xor	al,al
	out	dx,al

	call	print_nl
	pop	es			; = SYSSEG

; Check the ROM signature
	SEG_ES
	cmp	WLOC(0),CON(0xAA55)
	pop	es			; balance push/pop es
	je	sigok
	mov	cx,CON(badsigend-badsig)
	mov	si,CON(badsig)
	call	print_str
freeze:	JMP	freeze
sigok:

; Restore original disk parameters
	mov	bx,CON(0x78)
	mov	di,LOC(dpoff)
	mov	si,LOC(dpseg)
	xor	ax,ax
	mov	ds,ax
	mov	[bx],di
	mov	[bx+2],si

; after that (everything loaded), we jump to the .ROM file loaded.

	jmp	SYSSEG:6	; skip 2 bytes signature, 1 byte block count
				; jmp over and checksum byte

; This routine loads the system at address SYSSEG<<4, making sure no 64kB
; boundaries are crossed. We try to load it as fast as possible, loading whole
; tracks whenever we can.
;
; in:	es - starting address segment (normally SYSSEG)
;
read_it:
	mov	WLOC(sread),CON(1)
	mov	ax,es
	test	ax,CON(0x0fff)
die:	jne	die		; es must be at 64kB boundary
	xor	bx,bx		; bx is starting address within segment
rp_read:
	mov	ax,es
	mov	dx,bx
	mov	cl,CON(4)
	shr	dx,cl		; bx is always divisible by 16
	add	ax,dx
	cmp	ax,CON(SYSSEG+SYSSIZE)	; have we loaded all yet?
	jb	ok1_read
	ret
ok1_read:
	mov	ax,LOC(sectors)
	sub	ax,LOC(sread)
	mov	cx,ax
	shl	cx,CON(9)
	add	cx,bx
	jnc	ok2_read
	je	ok2_read
	xor	ax,ax
	sub	ax,bx
	shr	ax,CON(9)
ok2_read:
	call	read_track
	mov	cx,ax
	add	ax,LOC(sread)
	cmp	ax,LOC(sectors)
	jne	ok3_read
	mov	ax,CON(1)
	sub	ax,LOC(head)
	jne	ok4_read
	inc	WLOC(track)
ok4_read:
	mov	LOC(head),ax
	xor	ax,ax
ok3_read:
	mov	LOC(sread),ax
	shl	cx,CON(9)
	add	bx,cx
	jnc	rp_read
	mov	ax,es
	add	ah,CON(0x10)
	mov	es,ax
	xor	bx,bx
	JMP	rp_read

read_track:
	pusha
	push	ax
	push	bx
	push	bp			; just in case the BIOS is buggy
	mov	ax,CON(0x0e2e)		; 0x2e = .
	mov	bx,CON(0x0007)
	int	0x10
	pop	bp
	pop	bx
	pop	ax

	mov	dx,LOC(track)
	mov	cx,LOC(sread)
	inc	cx
	mov	ch,dl
	mov	dx,LOC(head)
	mov	dh,dl
	and	dx,CON(0x0100)
	mov	ah,CON(2)

	push	dx				; save for error dump
	push	cx
	push	bx
	push	ax

	int	0x13
	jc	bad_rt
	add	sp,BCON(8)
	popa
	ret

bad_rt:	push	ax				; save error code
	call	print_all			; ah = error, al = read

	xor	ah,ah
	xor	dl,dl
	int	0x13

	add	sp,BCON(10)
	popa
	JMP	read_track

; print_all is for debugging purposes.  It will print out all of the registers.
; The assumption is that this is called from a routine, with a stack frame like
;	dx
;	cx
;	bx
;	ax
;	error
;	ret <- sp

print_all:
	call	print_nl		; nl for readability
	mov	cx,CON(5)		; error code + 4 registers
	mov	bp,sp

print_loop:
	push	cx			; save count left

	cmp	cl,CON(5)
	jae	no_reg			; see if register name is needed

	mov	bx,CON(0x0007)		; page 0, attribute 7 (normal)
	mov	ax,CON(0xe05 + 0x41 - 1)	; 'A' - 1
	sub	al,cl
	int	0x10

	mov	al,CON(0x58)	; 'X'
	int	0x10

	mov	al,CON(0x3A)	; ':'
	int	0x10

no_reg:
	add	bp,BCON(2)		; next register
	call	print_hex		; print it
	mov	al,CON(0x20)		; print a space
	int	0x10
	pop	cx
	loop	print_loop
	call	print_nl		; nl for readability
	ret

print_str:
	mov	bx,CON(0x0007)		; page 0, attribute 7 (normal)
	mov	ah,CON(0x0e)		; write char, tty mode
prloop:
	lodsb
	int	0x10
	loop	prloop
	ret

print_nl:
	mov	bx,CON(0x0007)		; page 0, attribute 7 (normal)
	mov	ax,CON(0xe0d)		; CR
	int	0x10
	mov	al,CON(0xa)		; LF
	int	0x10
	ret

; print_hex prints the word pointed to by ss:bp in hexadecimal.

print_hex:
	mov	dx,[bp]			; load word into dx
	mov	cl,CON(4)
	mov	ah,CON(0x0e)		; write char, tty mode
	mov	bx,CON(0x0007)		; page 0, attribute 7 (normal)
	call	print_digit
	call	print_digit
	call	print_digit
; fall through
print_digit:
	rol	dx,cl		; rotate so that lowest 4 bits are used
	mov	al,CON(0x0f)	; mask for nybble
	and	al,dl
	add	al,CON(0x90)	; convert al to ascii hex (four instructions)
	daa
	adc	al,CON(0x40)
	daa
	int	0x10
	ret

sread:	dw 0			; sectors read of current track
head:	dw 0			; current head
track:	dw 0			; current track

sectors:
	dw 0

dpseg:	dw 0
dpoff:	dw 0

disksizes:
	db 36,18,15,9

msg1:
	db 13,10
	STRDECL('Loading ROM image')
msg1end:

badsig:
	STRDECL('No ROM signature')
badsigend:

#ifdef	USE_AS86
	org	510
#endif
#ifdef	USE_NASM
	times	510-($-$$) db 0
#endif

; The Etherboot loaders (which we loaded) store two words right below the
; 0xAA55 signature.  We need not reserve any space for that, because this is
; done after we finished our work.

	dw	0xAA55
