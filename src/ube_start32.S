/* #defines because ljmp wants a number, probably gas bug */
/*	.equ	KERN_CODE_SEG,_pmcs-_gdt	*/
#define	KERN_CODE_SEG	0x08
	.equ	KERN_DATA_SEG,_pmds-_gdt
/*	.equ	REAL_CODE_SEG,_rmcs-_gdt	*/
#define	REAL_CODE_SEG	0x18
	.equ	REAL_DATA_SEG,_rmds-_gdt
	.equ	CR0_PE,1

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

/*
 * NOTE: if you write a subroutine that is called from C code (gcc/egcs),
 * then you only have to take care of %ebx, %esi, %edi and %ebp.  These
 * registers must not be altered under any circumstance.  All other registers
 * may be clobbered without any negative side effects.  If you don't follow
 * this rule then you'll run into strange effects that only occur on some
 * gcc versions (because the register allocator may use different registers).
 *
 * All the data32 prefixes for the ljmp instructions are necessary, because
 * the assembler emits code with a relocation address of 0.  This means that
 * all destinations are initially negative, which the assembler doesn't grok,
 * because for some reason negative numbers don't fit into 16 bits. The addr32
 * prefixes are there for the same reasons, because otherwise the memory
 * references are only 16 bit wide.  Theoretically they are all superfluous.
 * One last note about prefixes: the data32 prefixes on all call _real_to_prot
 * instructions could be removed if the _real_to_prot function is changed to
 * deal correctly with 16 bit return addresses.  I tried it, but failed.
 */

	

/**************************************************************************
START - Where all the fun begins....
**************************************************************************/
/* this must be the first thing in the file because we enter from the top */
	.global	_start
	.code32
#ifdef IMAGE_MULTIBOOT
/**************************************************************************
XEND - Restart Etherboot from the beginning (from protected mode)
**************************************************************************/
	.globl xend
xend:
#endif
_start:
	cli

	cs;lgdt	gdtarg	
	ljmp	$KERN_CODE_SEG, $1f
1:
	/* reload other segment registers */
	movl	$KERN_DATA_SEG, %ebp
	movl	%ebp,%ds
	movl	%ebp,%es
	movl	%ebp,%ss
	movl	%ebp,%fs
	movl	%ebp,%gs

	movl	%esp, initesp
	movl	%eax, initeax
	movl	%ebx, initebx

	movl	$_estack, %esp

	call	main
	/* fall through */

	.globl	exit
exit:
	movl	initesp, %esp
	ret


/**************************************************************************
SETJMP - Save stack context for non-local goto
**************************************************************************/
	.globl	setjmp
setjmp:
	movl	4(%esp),%ecx
	movl	0(%esp),%edx
	movl	%edx,0(%ecx)
	movl	%ebx,4(%ecx)
	movl	%esp,8(%ecx)
	movl	%ebp,12(%ecx)
	movl	%esi,16(%ecx)
	movl	%edi,20(%ecx)
	movl	%eax,24(%ecx)
	movl	$0,%eax
	ret

/**************************************************************************
LONGJMP - Non-local jump to a saved stack context
**************************************************************************/
	.globl	longjmp
longjmp:
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	movl	0(%edx),%ecx
	movl	4(%edx),%ebx
	movl	8(%edx),%esp
	movl	12(%edx),%ebp
	movl	16(%edx),%esi
	movl	20(%edx),%edi
	cmpl	$0,%eax
	jne	1f
	movl	$1,%eax
1:	movl	%ecx,0(%esp)
	ret

/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
	.align	4
_gdt:
gdtarg:
	.word	0x27			/* limit */
	.long	_gdt			/* addr */
	.word	0

_pmcs:
	/* 32 bit protected mode code segment */
	.word	0xffff,0
	.byte	0,0x9f,0xcf,0

_pmds:
	/* 32 bit protected mode data segment */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0

_rmcs:
	/* 16 bit real mode code segment */
	.word	0xffff,(RELOC&0xffff)
	.byte	(RELOC>>16),0x9b,0x00,(RELOC>>24)

_rmds:
	/* 16 bit real mode data segment */
	.word	0xffff,(RELOC&0xffff)
	.byte	(RELOC>>16),0x93,0x00,(RELOC>>24)

initesp: .long 0
	.globl initeax
initeax: .long 0
	.globl initebx
initebx: .long 0


	.align	4

	.section ".bss"
	.p2align 3
	/* allocate a 4K stack in the bss segment */
_stack:
	.space 4096
_estack:
	
