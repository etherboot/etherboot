#ifndef PCBIOS
#error	"16bit code is only supported with the PCBIOS"
#endif
#define VERIFY_386 0		/* Costs 82 bytes */
	
#define	CODE_SEG 0x08
#define DATA_SEG 0x10

	.equ CR0_PE, 1

#ifdef	GAS291
#define DATA32 data32;
#define ADDR32 addr32;
#define	LJMPI(x)	ljmp	x
#else
#define DATA32 data32
#define ADDR32 addr32
/* newer GAS295 require #define	LJMPI(x)	ljmp	*x */
#define	LJMPI(x)	ljmp	x
#endif

	.text
	.code16
	.arch i386
	.org 0
	.globl _start
_start:
	/* trashes %eax, %ebx, and the segment registers
	 *
	 * %esp is converted into a physical address
	 */
#if VERIFY_386
	pushw	%ax			/* Save ax */
	pushf				/* Save flags */
/********************************************************************************
 * This CPU checking code is taken from syslinux. - Ken
 * Check that no moron is trying to boot Etherboot/32 on a 286 or so.  According
 * to Intel, the way to check is to see if the high 4 bits of the FLAGS register
 * are either all stuck at 1 (8086/8088) or all stuck at 0 (286 in real mode), if
 * not it is a 386 or higher.  A 186/188 behaves like a 8086/8088 in this check.
 ********************************************************************************
 */
	pushf				/* Get flags */
	popw	%ax
	andw	$0x0fff,%ax		/* Clear top 4 bits */
	pushw	%ax			/* Load into FLAGS */
	popf
	pushf				/* And load back */
	popw	%ax
	andw	$0xf000,%ax		/* Get top 4 bits */
	cmpw	$0xf000,%ax		/* If set -> 8086/8088 */
	je	not_386
test_286:
	pushf				/* Get flags */
	popw	%ax
	orw	$0xf000,%ax		/* Set top 4 bits */
	pushw	%ax
	popf
	pushf
	popw	%ax
	andw	$0xf000,%ax		/* Get top 4 bits */
	jnz	is_386			/* If not clear -> 386 */
not_386:
	movw	$err_not386_end-err_not386,%cx	/* length of string */
	movw	$err_not386-_start,%si
	movw	$0x0007,%bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e,%ah		/* write char, tty mode */
err_prloop:
	lodsb
	int	$0x10
	loop	err_prloop
	popf				/* Restore flags */
	popw	%ax			/* Restore ax */
	lret
is_386:
	popf				/* Restore flags */
	popw	%ax			/* Restore ax */
#endif

	/* Prepare the stack for 32bit code.
	 */
	pushf			/* Save the original interrupt state */
	movw	%sp, %ax
	andw	$0xffc, %sp	/* 32bit align the stack */
	pushw	%ss		/* Save the original stack address */
	pushw	%ax

	/*
	 * Disable Interrupts
	 */
	cli

	/*
	 * Compute the offset I am running at.
	 * and set ds
	 */
	movl	%cs, %ebx
	movw	%bx, %ds
	shll	$4, %ebx

	/*
	 * Fixup my 32bit pointers
	 */
	addl	%ebx, 2 + gdtarg - _start
	addl	%ebx, start32 - _start

	/* Switch to 32bit protected mode.
	 */
	ADDR32 lgdt	gdtarg-_start
	
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0

	DATA32	ljmp	*(start32 - _start)
	/* flush prefetch queue, and reload %cs:%eip */

#if VERIFY_386	
err_not386:
	.ascii	"Etherboot/32 requires 386+"
	.byte	0x0d, 0x0a
err_not386_end:
#endif /* VERIFY_386 */

start32:
	.long code32_start - _start
	.word CODE_SEG
	
_gdt:
gdtarg:
	.word	_gdt_end - _gdt - 1	/* limit */
	.long	_gdt - _start		/* addr */
	.word 0
_pmcs:
	/* 32 bit protected mode code segment */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0
_pmds:
	/* 32 bit protected mode data segment */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0
_gdt_end:

	.code32
code32_start:	
	/*
	 * Convert the stack pointer from 16 to 32 bit
	 */
	movl	%ss, %eax
	shll	$4, %eax
	movzwl	%sp, %esp
	addl	%eax, %esp
	
	/* Reload the segment registers */
	movl	 $DATA_SEG, %eax
	movl	 %eax, %ds
	movl	 %eax, %es
	movl	 %eax, %ss
	movl	 %eax, %fs
	movl	 %eax, %gs

	/* Remember I don't have a 32bit return address */
	pushl	$0

#if 1
	jmp	payload
	.balign 16
	/* Weird things happen if start16.bin isn't aligned
	 * to 16bytes so force it to be 112 bytes.  Hopefully we can
	 * stop careing at some point and just fall through.
	 */
#else
	/* Fall through and start executing my payload */
#endif
payload:
