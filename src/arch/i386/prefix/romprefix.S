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

/* Prefix exit codes.  We store these on the stack so that we will
 * know how to return control to the BIOS when Etherboot exits.
 */
#define EXIT_VIA_LRET  		0x0
#define EXIT_VIA_INT_18		0x1
#define EXIT_VIA_BOOT_INT	0x2

	.text
	.code16
	.arch i386
	.org 0
	.section ".prefix", "ax", @progbits
	.globl _prefix
_prefix: 
	.word 0xAA55			/* BIOS extension signature */
size:	.byte 0				/* number of 512 byte blocks */
					/* = number of 256 word blocks */
					/* filled in by makerom program */
	jmp	over			/* skip over checksum */
	.byte 0				/* checksum */
	jmp	legacyentry		/* alternate entry point +6 */
					/* used by mknbi-rom */

#ifdef	PCI_PNP_HEADER
mfgstr:
	.asciz	"Etherboot"
	
#ifdef	PXE_EXPORT
	.org	0x16
	.word	UNDIROMID - _prefix
#endif	/* PXE_EXPORT */
	
	.org	0x18
	.word	PCI - _prefix
	.word	PnP - _prefix

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
	.word mfgstr - _prefix
	.word 0x0			/* pointer to product name */
					/* filled by makerom */
	.byte 0x02			/* Device Base Type code */
	.byte 0x00			/* Device Sub-Type code */
	.byte 0x00			/* Device Interface Type code */
	.byte 0x14			/* device indicator */
	.word 0x0000			/* boot connection vector */
	.word 0x0000			/* disconnect vector */
	.word pnpentry - _prefix
	.word 0x0000			/* reserved */
	.word 0x0000			/* static resource information vector */
#ifdef	PXE_EXPORT
UNDIROMID:
	.ascii	"UNDI"
	.byte	UNDIROMID_end - UNDIROMID /* length of structure */
	.byte	0			/* Checksum */
	.byte	0			/* Structure revision */
	.byte	0,1,2			/* PXE version 2.1.0 */
	.word	UNDILoader - _prefix	/* Offset to loader routine */
	.word	UNDIStackSize		/* Stack segment size */
	.word	UNDIDataSize		/* Data segment size */
	.word	UNDICodeSize		/* Code segment size */
	.ascii	"PCIR"

	/* The code segment contains our pxe_stack_t plus the PXE and
	 * RM callback interfaces.  We don't actually use a data
	 * segment, but we put a nonzero value here to avoid confusing
	 * things.  16k of stack space should be enough.
	 *
	 * When we claim our own memory, we fill out the data segment
	 * with the address and size of the real-mode stack, so that
	 * NBPs will free that area of memory for us.  When the UNDI
	 * loader is used to initialise us, we will never need a
	 * real-mode stack because we will only ever be called via the
	 * PXE API, hence our stack is already in base memory.
	 */
	.equ	UNDICodeSize, _pxe_stack_size
	.equ	UNDIDataSize, 0x1
	.equ	UNDIStackSize, 0x1000
UNDIROMID_end:	
#endif	/* PXE_EXPORT */
	
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
	movw	$start_int - _prefix, %ax
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
	pushl	%eax			/* padding */
	pushw	$EXIT_VIA_BOOT_INT
	jmp	invoke
#endif	/* DELAYED_INT */



	
legacyentry:
	pushw	$EXIT_VIA_LRET
	jmp	invoke

	
	
#ifdef PCI_PNP_HEADER
pnpentry:
	pushl	%eax			/* padding */
	pushw	$EXIT_VIA_INT_18
	jmp	invoke
#endif /* PCI_PNP_HEADER */


invoke:	
	/* Relocate to free base memory, switch stacks */
	pushw	$6			/* Preserve exit code & far ret addr */
	call	prelocate
	/* leave "6" on stack as padding out to 4-byte alignment */
	pushl	$8			/* Preserve exit code & far ret addr */
	pushw	$0			/* Set null return address */
	jmp	_start
	

	.section ".text16", "ax", @progbits
	.globl	prefix_exit
prefix_exit:
	popw	%ax			/* padding */
	popw	%ax			/* %ax = exit code */
	cmpw	$EXIT_VIA_LRET, %ax
	jne	1f
	/* Exit via LRET */
	lret
1:	addw	$4, %sp			/* Strip padding */
	cmpw	$EXIT_VIA_BOOT_INT, %ax
	jne	2f
	/* Exit via int BOOT_INT */
	int	$BOOT_INT		/* Try original vector */
2:	/* Exit via int $0x18 */
	int	$0x18			/* As per BIOS Boot Spec, next dev */
	.globl	prefix_exit_end
prefix_exit_end:
	.previous

	
	
#ifdef 	PXE_EXPORT
	.section ".prefix"
	/* Placeholder UNDI loader routine: just print out a message
	 * and return failure.
	 */
loadermsg:	.ascii "Dummy UNDI loader called\r\n"
loadermsg_end:	
UNDILoader:
	pushw	%cs
	popw	%ds
	movw	$loadermsg - _prefix, %si
	movw	$loadermsg_end - loadermsg, %cx
	movw	$0x0007, %bx
	movb	$0x0e, %ah
prloop:	
	lodsb
	int	$0x10
	loop	prloop
	/* Return failure */
	movw	$1, %ax
	lret
#endif	/* PXE_EXPORT */
		
