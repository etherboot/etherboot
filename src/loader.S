/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 */

#ifdef	ZLOADER
/* LZHuf (LZSS) Decompressing boot loader for ROM images
 *
 * this code is based on the work of Haruyasu Yoshizaki and Haruhiko Okumura
 * who implemented the original compressor and decompressor in C code
 *
 * Copyright 1997 by M. Gutschke <gutschk@math.uni-muenster.de>
 *
 * Compression pays off, as soon as the uncompressed image is bigger than
 * about 1.5kB. This assumes an average compressibility of about 60%.
 */
#endif

#ifdef	PXELOADER
#ifdef	PCI_PNP_HEADER
#error Cannot have PXELOADER and PCI_PNP_HEADER
#endif
#endif

/* This provides for execution at 0x88000 to avoid disk-on-chip drivers */
#undef	NOT_AT_0x9xxxx
#if	(RELOC & 0xF0000) != 0x90000
#define NOT_AT_0x9xxxx
#else
#ifdef	PXELOADER
/* also don't depend on the amount of memory int 12 reports as it has been 
   decreased by PXE and not increased again even after releasing it */
#define NOT_AT_0x9xxxx
#endif / * PXELOADER */
#define	MAX_SP	((RELOC & 0xF0000) + 0x10000 - RELOC)
/* e.g. RELOC = 0x84000, MAX_SP = 0x90000 - 0x84000 = 0xC000 */
#endif

/* If the Makefile does not provide configuration options, then set
 * reasonable default values.
 */
#ifdef	MOVEROM
#undef	MOVEROM
/* Check if we are running from somewhere other than 0x9xxxx */
#ifdef	NOT_AT_0x9xxxx
#define MOVEROM		0x70000
#else
#define MOVEROM		0x80000
#endif
#endif

/* Scratch space has to be at least 10kB. With the start
 * address at 0x94000 we cannot use more then 48kB for uncompressed data
 * anyways, so lets give all the rest of the segment to the scratch area.
 */
#ifndef SCRATCH
#define SCRATCH		0x8000
#endif

/* We need some unique magic ID, if we defer startup thru the INT18H or INT19H
 * handler. This way, we can check if we have already been installed.
 */
#ifndef MAGIC
#define MAGIC		0xE44C
#endif

/* Hook into INT18H or INT19H handler */
#ifdef	BOOT_INT18H
#define BOOT_INT	0x18
#else
#define BOOT_INT	0x19
#endif

#define BOOT_INT_VEC	BOOT_INT*4
#define SCRATCHVEC	0x300

/* Offsets of words containing ROM's CS and size (in 512 byte blocks)
 * from start of floppy boot block at 0x7c00
 * Offsets must match those in etherboot.h
 */
#define FLOPPY_SEGMENT	0x7c0
#define ROM_SEGMENT	0x1fa
#define ROM_LENGTH	0x1fc

/* Do not change these values unless you really know what you are doing
 * the pre-computed lookup tables rely on the buffer size being 4kB or
 * smaller. The buffer size must be a power of two. The lookahead size has
 * to fit into 6 bits. If you change any of these numbers, you will also
 * have to adjust the compressor accordingly.
 */
#define BUFSZ		4096
#define LOOKAHEAD	60
#define THRESHOLD	2
#define NCHAR		(256+LOOKAHEAD-THRESHOLD)
#define TABLESZ		(NCHAR+NCHAR-1)
#define ROOT		(TABLESZ-1)

	.text
	.code16
	.org 0

_start: 
#ifndef	PXELOADER
	.word 0xAA55			/* BIOS extension signature */
size:	.byte 0				/* number of 512 byte blocks */
					/* = number of 256 word blocks */
					/* filled in by makerom program */
	jmp	over			/* skip over checksum */
	.byte 0				/* checksum */
	jmp	legacyentry		/* alternate entry point +6 */
					/* used by floppyload and comload */

#ifdef	PCI_PNP_HEADER
mfgstr:
	.asciz	"Etherboot"
	.fill	0x18-(.-_start)
	.word	PCI
	.word	PnP

PCI: 
	.ascii	"PCIR"
	.word 0x8086			/* vendor ID, filled in by makerom */
	.word 0x1229			/* device ID, filled in by makerom */
	.word 0x0000			/* pointer to vital product data */
	.word 0x0018			/* PCI data structure length */
	.byte 0x00			/* PCI data structure revision */
	.byte 0x02			/* Device Base Type code */
	.byte 0x00			/* Device Sub-Type code */
	.byte 0x00			/* Device Interface Type code */
	.word 0x0000			/* Image length same as offset 02h */
	.word 0x0001			/* revision level of code/data */
	.byte 0x00			/* code type */
	.byte 0x80			/* indicator (last PCI data structure) */
	.word 0x0000			/* reserved */

PnP:
	.ascii	"$PnP"
	.byte 0x01			/* structure revision */
	.byte 0x02			/* length (in 16 byte increments) */
	.word 0x0000			/* offset of next header */
	.byte 0x00			/* Reserved */
	.byte 0x00			/* checksum filled by makerom */
	.long 0x00000000		/* Device identifier */
	.word mfgstr
	.word 0x0			/* pointer to product name */
					/* filled by makerom */
	.byte 0x02			/* Device Base Type code */
	.byte 0x00			/* Device Sub-Type code */
	.byte 0x00			/* Device Interface Type code */
	.byte 0x14			/* device indicator */
	.word 0x0000			/* boot connection vector */
	.word 0x0000			/* disconnect vector */
	.word pnpentry
	.word 0x0000			/* reserved */
	.word 0x0000			/* static resource information vector */
#endif	/* PCI_PNP_HEADER */

over: 
	cmpw	$'$'+'P'*256,(%di)
	jne	notpnp
	cmpw	$'n'+'P'*256,2(%di)
	jne	notpnp
	movw	$0x20,%ax
	lret
notpnp:
#ifndef NO_DELAYED_INT
	pushw	%ax
	pushw	%ds
	xorw	%ax,%ax
	movw	%ax,%ds			/* access first 64kB segment */
	movw	SCRATCHVEC+4, %ax	/* check if already installed */
	cmpw	$MAGIC, %ax		/* check magic word */
	jz	installed
	movw	BOOT_INT_VEC, %ax	/* hook into INT18H or INT19H */
	movw	%ax, SCRATCHVEC
	movw	BOOT_INT_VEC+2, %ax
	movw	%ax, SCRATCHVEC+2
	movw	$start_int, %ax
	movw	%ax, BOOT_INT_VEC
	movw	%cs,%ax
	movw	%ax, BOOT_INT_VEC+2
	movw	$MAGIC, %ax		/* set magic word */
	movw	%ax, SCRATCHVEC+4
installed:
	popw	%ds
	popw	%ax
	movw	$0x20,%ax
	lret

start_int:				/* clobber magic id, so that we will */
	xorw	%ax,%ax			/* not inadvertendly end up in an */
	movw	%ax,%ds			/* endless loop */
	movw	%ax, SCRATCHVEC+4
	movw	SCRATCHVEC+2, %ax	/* restore original INT19h handler */
	movw	%ax, BOOT_INT_VEC+2
	movw	SCRATCHVEC, %ax
	movw	%ax, BOOT_INT_VEC
#endif	/* NO_DELAYED_INT */
legacyentry:
	movw	$MAGIC,%ax
	jmp	blockmove
pnpentry:
	movw	$0,%ax
#else /* PXELOADER */
#define PXENV_UNDI_SHUTDOWN	0x05
#define PXENV_UNLOAD_STACK	0x70
	jmp	startpxe
size:	.byte 64			/* number of 512 byte blocks */
					/* PXE will not load more than 32K */
					/* so this should be <= 64 */
					/* filled in by makerom program */
startpxe:
	jmp	$FLOPPY_SEGMENT, $n-_start
n:
	movw	%sp, %bp

	push	%cs
	pop	%ds
	
	movw	$hellomsgend-hellomsg, %cx
	movw	$hellomsg-_start, %si
	movw	$0x0007, %bx			/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah			/* write char, tty mode */
prloop_hellomsg: 
	lodsb
	int	$0x10
	loop	prloop_hellomsg
	
	movw	$MAGIC,%ax
	jmp	pxefind
hellomsg:
	.ascii	"PXE loader for etherboot"
#if	defined(ZLOADER) 
	.ascii	"(with ZLOADER)"
#endif
	.ascii	"\r\n"
hellomsgend: 
pxe_undi_shutdown_pkt:
	.word	0
pxe_unload_stack_pkt:
	.word	0
	.word	0,0,0,0,0
PXEEntry:
	.word	0,0
pxefind:
	les	4(%bp), %di		/* !PXE structure */
	cmpl	$0x45585021, %es:(%di)	/* '!PXE' signature */
	je	have_pxe
	movl	$0x202b564e, pxemsg2-_start /* 'NV+ ' */
	mov	$0x5650, %ax
	int	$0x1a
	jc	no_pxe
	cmp	$0x564e, %ax
	jne	no_pxe

	cmpl	$0x4e455850, %es:(%bx)	/* 'PXEN' signature */
	jne	no_pxe
	cmpw	$0x2b56, %es:4(%bx)	/* 'V+' signature */
	jne	no_pxe
	mov	%bx, %cs:PXEEntry  -_start /* save them in case we use PXENV+ */
	mov	%es, %cs:PXEEntry+2-_start
	cmpw	$0x201, %es:6(%bx)	/* API version 2.1 or higher */
	jae	get_pxe			/* otherwise use PXENV+ */
use_pxenv:
	les	%es:0x0a(%bx), %di
	mov	%di, %cs:PXEEntry  -_start
	mov	%es, %cs:PXEEntry+2-_start
	push	%ds
	pop	%es
	mov	$pxe_undi_shutdown_pkt, %di
	mov	$PXENV_UNDI_SHUTDOWN, %bx
	lcall	%cs:(PXEEntry-_start)
	mov	$pxe_unload_stack_pkt, %di
	mov	$PXENV_UNLOAD_STACK, %bx
	lcall	%cs:(PXEEntry-_start)
	jmp	pxe
get_pxe:
	les	%es:0x28(%bx), %di
	cmpl	$0x45585021, %es:(%di)	/* '!PXE' signature */
	jne	no_pxe
	jmp	have_pxe
/*	mov	%cs:PXEEntry+2-_start, %bx /* restore PXENV+ */
/*	mov	%bx, %es
	mov	%cs:PXEEntry  -_start, %bx
	jmp	use_pxenv */
have_pxe:
	les	%es:0x10(%di), %di
	mov	%di, %cs:PXEEntry  -_start
	mov	%es, %cs:PXEEntry+2-_start

	pushw	%cs
	pushw	$pxe_undi_shutdown_pkt
	pushw	$PXENV_UNDI_SHUTDOWN
	lcall	%cs:(PXEEntry-_start)
	add	$6, %sp
	pushw	%cs
	pushw	$pxe_unload_stack_pkt
	pushw	$PXENV_UNLOAD_STACK
	lcall	%cs:(PXEEntry-_start)
	add	$6, %sp

pxe:	movw	$pxemsg-_start+3, %si
	movw	$pxemsgend-pxemsg-3, %cx
	jmp	pxe_print
no_pxe:
	movw	$pxemsg-_start, %si
	movw	$pxemsgend-pxemsg, %cx
pxe_print:
	movw	$0x0007, %bx			/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah			/* write char, tty mode */
prloop_pxemsg: 
	lodsb
	int	$0x10
	loop	prloop_pxemsg
	
	movw	$MAGIC,%ax
	jmp	blockmove
pxemsg:
	.ascii	"No PXE"
pxemsg2:
	.ascii	"    unloaded\r\n"
pxemsgend:
#endif /* PXELOADER*/
blockmove: 
	pushw	%ax			/* indicates whether PnP or legacy */
/* The following copy is a bit convoluted to save source code, so here a few
 * lines on the why of all this. The standard (non-compressed) loader needs to
 * copy its payload to the RELOC:0 area in any case. The compressed loader
 * offers an option to move the whole ROM contents to RAM (at MOVEROM:0) before
 * excuting it. This is just a speed improvement on systems where the ROM area
 * is not cached. The compressed loader (no matter if it executes from RAM or
 * ROM) decompresses its payload to RELOC:0.
 */
	xorw	%cx,%cx
	movb	%cs:size-_start, %ch

/* Save ROMs CS and length in floppy boot block */
	pushw	%ds			/* save DS */
	movw	$FLOPPY_SEGMENT, %ax
	movw	%ax,%ds
	movw	%cs,%ax
	movw	%ax, ROM_SEGMENT
	movw	%cx, ROM_LENGTH
	popw	%ds			/* restore DS */

/* PXE loads this code in FLOPPY_SEGMENT so we have to leave some room
   or it will clobber code */
#ifdef	PXELOADER
	jmp	endrominfo
	.org	ROM_SEGMENT
	.word	0
	.word	0
	.word	0xAA55			/* bootblock signature */
endrominfo:
#endif	/* PXELOADER */

#if	!defined(ZLOADER) || defined(MOVEROM)
	cld
#ifdef	ZLOADER /* copy full ROM contents to RAM to improve execution speed */
	movw	$MOVEROM>>4, %ax
	xorw	%si,%si			/* si = 0 */
#else		/* copy only payload to RELOC:0 */
	movw	$RELOC>>4, %ax
	movw	$payload-_start, %si	/* offset of code image */
	movw	%si,%bx
	shrw	%bx
	subw	%bx,%cx			/* calculate legth of payload */
#endif
	movw	%ax,%es
	xorw	%di,%di			/* di = 0 */
	rep movsw	%cs:(%si),%es:(%di)

#ifdef	ZLOADER
	jmp	$MOVEROM>>4, $moved	/* reload cs:ip to execute from RAM */
moved: 
#endif
#endif

/* Change segment registers and stack */
	movw	$RELOC>>4, %bx		/* new ss */
#ifdef	ZLOADER
	movw	%ax,%ds			/* new ds (loaded above from cs) */
	movw	$(RELOC-SCRATCH)>>4, %ax /* first 32kB -> scratch space */
	movw	%ax,%es			/* new es (for data copying in init) */
#else
	movw	%bx,%ds			/* new ds */
#endif
#ifdef	NOT_AT_0x9xxxx
/* if we are not at 0x9xxxx, then don't use INT12 to set stack,
   instead, set stack at top of 64k band */
	movw	$MAX_SP, %ax
#else
	int	$0x12			/* get conventional memory size in KB */
	movb	$6, %cl
	shlw	%cl,%ax			/* 8086 cannot do shl ax,6! */
	subw	%bx,%ax			/* ax = (top of mem - RELOC) / 16 */
#ifndef DONT_CHECK_STACKOFFSET
	testw	$0xf000, %ax		/* would sp wrap the segment limit? */
	jz	stackok
rev:	js	rev			/* segment is too short, just crash */
	movw	$0x0000, %ax		/* use maximum stack pointer */
/* Note this only works because the stack contains at least a return address
 * while in protected mode, otherwise the different wraparound behaviour in
 * real and protected mode would result in random crashes.
 */
stackok: 
#endif
	movb	$4, %cl			/* ax *= 16 */
	shlw	%cl,%ax			/* new sp */
#endif	/* NOT_AT_0x9xxxx */
	popw	%dx			/* pop PnP marker */
	movw	%bx,%ss
	movw	%ax,%sp
	pushw	%dx			/* put PnP marker on new stack */

#ifdef	ZLOADER
/*
 * INIT -- initializes all data structures
 * ====
 */

init:	
	cld
	movw	$dcodrle-_start, %si	/* uncompress run length encoded */
	movw	$dcode, %di		/* lookup table for codes */
	movb	$6, %dl
	movb	$0x20, %dh
	xorb	%bh,%bh
init0:	
	lodsb
	movb	%al,%bl
init1:	
	movb	%dh,%cl
	xorb	%ch,%ch
	movb	%bh,%al
	rep
	stosb
	incb	%bh
	decb	%bl
	jnz	init1
	shrb	%dh
	decb	%dl
	jnz	init0
	movb	$1, %bl			/* uncompress run length encoded */
	movb	$6, %bh			/* lookup table for code lengths */
init2:	
	lodsb
	movb	%al,%cl
	xorb	%ch,%ch
	movb	%bl,%al
	rep
	stosb
	incb	%bl
	decb	%bh
	jnz	init2
	movw	%es,%ax			/* we no longer have to access static */
	movw	%ax,%ds			/* data, so set segment accordingly */
	movw	$NCHAR, %cx		/* set all frequencies of leaf nodes */
	movw	$1, %ax			/* to one */
	rep
	stosw
	movw	$freq, %si
	movw	$ROOT+1-NCHAR, %cx
init3:	
	lodsw				/* update frequencies of non-leaf nodes */
	movw	%ax,%bx
	lodsw
	addw	%bx,%ax
	stosw
	loop	init3
	movw	$0xFFFF, %ax
	stosw				/* sentinel with infinite frequency */
	movw	$NCHAR, %cx
	movw	$TABLESZ, %ax
init4:	
	stosw				/* update son pointers for leaf nodes */
	incw	%ax
	loop	init4
	movw	$ROOT+1-NCHAR, %cx
	xorw	%ax,%ax
init5:	
	stosw				/* update son ptrs for non-leaf nodes */
	addw	$2, %ax
	loop	init5
	movw	$ROOT+1-NCHAR, %cx
	movw	$NCHAR, %ax
init6:	
	stosw				/* update parent ptrs for non-leaf nd. */
	stosw
	incw	%ax
	loop	init6
	movw	$NCHAR, %cx
	xorw	%ax,%ax
	stosw				/* root node has no parent */
init7:	
	stosw				/* update parent ptrs for leaf nodes */
	incw	%ax
	loop	init7
	xorw	%ax,%ax
	stosb				/* clear getlen */
	stosw				/* clear getbuf */
	movb	$0x20, %al		/* fill text buffer with spaces */
	movw	$spaces, %di
	movw	$BUFSZ-LOOKAHEAD, %cx
	rep
	
	stosb
	/* fall thru */


/* 
 * MAIN -- reads compressed codes and writes decompressed data
 * ====
 */

	movw	$payload-_start, %si	/* get length of compressed data stream */
	movw	$uncompressed, %di
/*	SEG_CS */
	.byte	0x2e
	lodsw
	movw	%ax,%cx
	lodsw				/* cannot do more than 64k anyways */
main1:	
	pushw	%cx
	call	dcdchr			/* decode one code symbol */
	orb	%ah,%ah			/* test if 8bit character */
	jnz	main2
	stosb				/* store verbatim */
	popw	%cx
	loop	main1			/* proceed with next compressed code */
	jmp	done			/* until end of input is detected */
main2:	
	pushw	%ax
	call	dcdpos			/* compute position in output buffer */
	movw	%si,%ax
	subw	%di,%bx
	notw	%bx
	movw	%bx,%si			/* si := di - dcdpos() - 1 */
	popw	%cx
	subw	$255-THRESHOLD, %cx	/* compute length of code sequence */
	movw	%cx,%dx
	rep
	movsb
	movw	%ax,%si
	popw	%cx
	subw	%dx,%cx			/* check end of input condition */
	jnz	main1			/* proceed with next compressed code */
done: 
	movw	$RELOC>>4, %ax		/* set ds then call etherboot */
	movw	%ax,%ds
	movw	%ax,%es
#endif	/* ZLOADER */

#ifdef	ENTRYPOINT
	call	ENTRYPOINT
#else
	call	$RELOC>>4, $0
#endif
	popw	%ax
#ifdef	PCI_PNP_HEADER
	cmpw	$MAGIC,%ax
	jne	pnpreturn
#endif	/* PCI_PNP_HEADER */
#ifdef	NO_DELAYED_INT
	lret
#else
	int	$BOOT_INT		/* Try original vector */
#endif
pnpreturn:
	int	$0x18			/* As per BIOS Boot Spec, next dev */

#ifdef	ZLOADER
/*
 * GETBIT -- gets one bit pointed to by DS:SI
 * ======
 *
 * changes: AX,CX,DL
 */

getbit:
	movb	$8, %cl
	movb	getlen, %dl		/* compute number of bits required */
	subb	%dl,%cl			/* to fill read buffer */
	jae	getbit1
	movw	getbuf, %ax		/* there is still enough read ahead data */
	jmp	getbit2
getbit1:
/*	SEG_CS */
	.byte	0x2e
	lodsb				/* get next byte from input stream */
	xorb	%ah,%ah
	shlw	%cl,%ax			/* shift, so that it will fit into */
	movw	getbuf, %cx		/* read ahead buffer */
	orw	%cx,%ax
	addb	$8, %dl			/* update number of bits in buffer */
getbit2:
	movw	%ax,%cx
	shlw	%cx			/* extract one bit from buffer */
	movw	%cx, getbuf
	decb	%dl
	movb	%dl, getlen		/* and update number of bits */
	shlw	%ax			/* return in carry flag */
	ret


/*
 * DCDPOS -- decodes position in textbuffer as pointed to by DS:SI, result in BX
 * ======
 *
 * changes: AX,BX,CX,DX
 */

dcdpos:
	movw	$0x0800, %bx
dcdpos1:
	shlb	%bl			/* read one byte */
	call	getbit
	jnc	dcdpos2
	incb	%bl
dcdpos2:
	decb	%bh
	jnz	dcdpos1
	movb	%bl,%dh			/* read length of code from table */
	xorb	%bh,%bh
	movb	dlen(%bx),%cl
	xorb	%ch,%ch
	movb	dcode(%bx),%bl		/* get top six bits from table */
	shlw	%bx
	shlw	%bx
	shlw	%bx
	shlw	%bx
	shlw	%bx
	shlw	%bx
dcdpos3:
	pushw	%cx			/* read the rest from the input stream */
	shlb	%dh
	call	getbit
	jnc	dcdpos4
	incb	%dh
dcdpos4:
	popw	%cx
	loop	dcdpos3
	andb	$0x3f, %dh		/* combine upper and lower half of code */
	orb	%dh,%bl
	ret

/*
 * DCDCHR -- decodes one compressed character pointed to by DS:SI
 * ======
 *
 * changes: AX,BX,CX,DX
 */

dcdchr:
	movw	$ROOT, %bx		/* start at root entry */
	shlw	%bx
	movw	son(%bx),%bx
dcdchr1:
	call	getbit			/* get a single bit */
	jnc	dcdchr2
	incw	%bx			/* travel left or right */
dcdchr2:
	shlw	%bx
	movw	son(%bx),%bx
	cmpw	$TABLESZ, %bx		/* until we come to a leaf node */
	jb	dcdchr1
	movw	%bx,%ax
	subw	$TABLESZ, %ax
	/* fall thru */

/*
 * UPDATE -- updates huffman tree after incrementing frequency for code in BX
 * ======
 *
 * changes: BX,CX,DX
 */

update:
	/* we do not check whether the frequency count has overrun.
	 * this will cause problems for large files, but be should be fine
	 * as long as the compressed size does not exceed 32kB and we
	 * cannot do more than this anyways, because we load into the
	 * upper 32kB of conventional memory
	 */
	pushw	%si
	pushw	%ax
	shlw	%bx
	movw	parent(%bx),%bx
update1:
	shlw	%bx
	movw	freq(%bx),%dx
	incw	%dx			/* increment frequency count by one */
	movw	%dx,freq(%bx)
	movw	%bx,%si
	addw	$freq+2, %si
	lodsw				/* check if nodes need reordering */
	cmpw	%ax,%dx
	jbe	update5
update2:
	lodsw
	cmpw	%dx,%ax
	jb	update2
	movw	-4(%si),%cx
	movw	%cx,freq(%bx)		/* swap frequency of entries */
	movw	%dx,-4(%si)
	movw	%si,%ax			/* compute index of new entry */
	subw	$freq+4, %ax
	movw	%ax,%dx
	shrw	%ax
	movw	son(%bx),%cx		/* get son of old entry */
	movw	%cx,%si
	addw	%si,%si
	movw	%ax,parent(%si)		/* and update the ptr to new parent */
	cmpw	$TABLESZ, %cx
	jae	update3			/* do this for both branches */
	movw	%ax,parent+2(%si)	/* if not a leaf node */
update3:
	movw	%dx,%si
	movw	son(%si),%dx		/* get son of new entry */
	movw	%cx,son(%si)		/* update its contents */
	movw	%dx,%si
	addw	%si,%si
	movw	%bx,%cx
	shrw	%cx
	movw	%cx,parent(%si)		/* and update the ptr to new paren */
	cmpw	$TABLESZ, %dx
	jae	update4			/* do this for both branches */
	movw	%cx,parent+2(%si)	/* if not a leaf node */
update4:
	movw	%dx,son(%bx)		/* update son of old entry */
	movw	%ax,%bx			/* continue with new entry */
	shlw	%bx
update5:
	movw	parent(%bx),%bx		/* continue with parent */
	orw	%bx,%bx
	jnz	update1			/* until we found the root entry */
	popw	%ax
	popw	%si
	ret

/*
 * constant data. this part of the program resides in ROM and cannot be
 * changed
 *
 * run length encoded tables will be uncompressed into the bss segment
 * take care with any symbols here for .com files to add 0x100 to address
 */

dcodrle: .byte 0x01,0x03,0x08,0x0C,0x18,0x10
dlenrle: .byte 0x20,0x30,0x40,0x30,0x30,0x10

/*
 * variable data segment (bss)
 * this segment will always be found at 0x90000 (i.e. at RELOC - SCRATCH)
 *
 * do not change the order or the sizes of any of the following tables
 * the initialization code makes assumptions on the exact layout of the
 * data structures...
 */

/* lookup table for index into buffer of recently output characters */

.equ	dcode,	0

/* lookup table for length of code sequence from buffer of recent characters */

.equ	dlen,	dcode+256

/* table with frequency counts for all codes */
.equ	freq,	dlen+256

/* pointer to child nodes */
.equ	son,	freq+2*(TABLESZ+1)

/* the first part of this table contains all the codes	(0..TABLESZ-1) */
/* the second part contains all leaf nodes		(TABLESZ..) */
.equ	parent, son+2*(TABLESZ)

/* temporary storage for extracting bits from compressed data stream */
.equ	getlen, parent+2*(TABLESZ+NCHAR)
.equ	getbuf, getlen+1

/* the initial buffer has to be filled with spaces (size: BUFSZ+LOOKAHEAD) */
.equ	spaces, SCRATCH-BUFSZ+LOOKAHEAD

/* uncompressed data will be written to address 0x98000 */

.equ		uncompressed, SCRATCH
#endif	/* ZLOADER */

/* Force 4 byte alignment */
	.align 4, 0
payload: 
/* the (compressed) code will be attached here */
