/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 *
 * Also be very careful as the stack is at the rear end of the interrupt
 * table so using a noticeable amount of stack space is a no-no.
 */

/* Define DELAYED_INT when NO_DELAYED_INT is not defined.
 * This allows positive tests instead of tests that contain
 * double negatives, and become confusing.
 */
#ifndef NO_DELAYED_INT
#define DELAYED_INT
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
	.globl _start
_start: 
	.word 0xAA55			/* BIOS extension signature */
size:	.byte 0				/* number of 512 byte blocks */
					/* = number of 256 word blocks */
					/* filled in by makerom program */
	jmp	over			/* skip over checksum */
	.byte 0				/* checksum */
	jmp	legacyentry		/* alternate entry point +6 */
					/* used by floppyload */

#ifdef	PCI_PNP_HEADER
mfgstr:
	.asciz	"Etherboot"
	.org	0x18
	.word	PCI - _start
	.word	PnP - _start

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
	.word mfgstr - _start
	.word 0x0			/* pointer to product name */
					/* filled by makerom */
	.byte 0x02			/* Device Base Type code */
	.byte 0x00			/* Device Sub-Type code */
	.byte 0x00			/* Device Interface Type code */
	.byte 0x14			/* device indicator */
	.word 0x0000			/* boot connection vector */
	.word 0x0000			/* disconnect vector */
	.word pnpentry - _start
	.word 0x0000			/* reserved */
	.word 0x0000			/* static resource information vector */
#endif	/* PCI_PNP_HEADER */

/*
 *	Explicitly specify DI is wrt ES to avoid problems with some BIOSes
 *	Discovered by Eric Biederman
 *	In addition, some BIOSes don't point DI to the string $PnP so
 *	we need another #define to take care of that.
 */
over: 
/* Omit this test for ISA cards anyway */
#ifdef	PCI_PNP_HEADER
/* Accept old name too for backward compatibility */
#if	!defined(BBS_BUT_NOT_PNP_COMPLIANT) && !defined(PNP_BUT_NOT_BBS_COMPLIANT)
	cmpw	$'$'+'P'*256,%es:0(%di)
	jne	notpnp
	cmpw	$'n'+'P'*256,%es:2(%di)
	jne	notpnp
#endif	/* BBS_BUT_NOT_PNP_COMPLIANT */
	movw	$0x20,%ax
	lret
#endif	/* PCI_PNP_HEADER */
notpnp:
#ifdef  DELAYED_INT
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
	movw	$start_int - _start, %ax
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
#endif	/* DELAYED_INT */
legacyentry:
	movw	$MAGIC,%ax
	jmp	blockmove
pnpentry:
	movw	$0,%ax
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

/* Save ROMs CS and length in floppy boot block */
	xorw	%cx,%cx
	movb	%cs:size-_start, %ch
#if 1
	/* Read the ROM_INFO for now, we can't do anything
	 * productive with it, but it doesn't cost much and I
	 * don't feel like removing it today.
	 * --Eric Biederman 23 August 2002
	 */
	pushw	%ds			/* save DS */
	movw	$FLOPPY_SEGMENT, %ax
	movw	%ax,%ds
	movw	%cs,%ax
	movw	%ax, ROM_SEGMENT
	movw	%cx, ROM_LENGTH
	popw	%ds			/* restore DS */
#endif

	cld

	/* copy payload to RELOC */
	movw	$((RELOC - S16SIZE)>>4), %ax
	movw	$payload-_start, %si	/* offset of code image */
	movw	%si,%bx
	shrw	%bx
	subw	%bx,%cx			/* calculate legth of payload */

	movw	%ax,%es
	xorw	%di,%di			/* di = 0 */
	rep movsw	%cs:(%si),%es:(%di)

	cli

	lcall	$((RELOC - S16SIZE) >>4), $0

	popw	%ax
#ifdef	PCI_PNP_HEADER
	cmpw	$MAGIC, %ax
	jne	pnpreturn
#endif
#ifdef	DELAYED_INT
	int	$BOOT_INT		/* Try original vector */
#else
	lret
#endif
pnpreturn:
	int	$0x18			/* As per BIOS Boot Spec, next dev */

/* Force 4 byte alignment */
	.balign 4, 0
payload: 
/* the (compressed) code will be attached here */
