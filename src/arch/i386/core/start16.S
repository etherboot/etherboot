#ifndef PCBIOS
#error	"16bit code is only supported with the PCBIOS"
#endif

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

	jmp	payload
	.balign 16
	/* Etherboot needs to be 16byte aligned or data that
	 * is virtually aligned is no longer physically aligned
	 * which is just nasty in general.  16byte alignment 
	 * should be sufficient though.
	 */
payload:
