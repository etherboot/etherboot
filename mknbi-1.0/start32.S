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
#define	LJMPI(x)	ljmp	*x
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
_start:
	.code16
/**************************************************************************
This CPU checking code is taken from syslinux. - Ken
Check that no moron is trying to boot Etherboot/32 on a 286 or so.  According
to Intel, the way to check is to see if the high 4 bits of the FLAGS register
are either all stuck at 1 (8086/8088) or all stuck at 0 (286 in real mode), if
not it is a 386 or higher.  A 186/188 behaves like a 8086/8088 in this check.
**************************************************************************/
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
	int	$0x19		/* used to be lret */
is_386:
	movw	%ss,%bx
	movw	%sp,%cx
	DATA32 call	_real_to_prot
	.code32
	movw	%bx,initss
	movw	%cx,initsp

	call	main
	/* fall through */

	.globl	exit
exit:
	movw	initss,%bx
	movw	initsp,%cx
	call	_prot_to_real
	.code16
/*	we reset sp to the location just before entering main
	instead of relying on the return from main because exit
	could have been called from anywhere */
	movw	%bx,%ss
	movw	%cx,%sp
	int	$0x19		/* used to be lret */
	.code32

initss:
	.word	0
initsp:
	.word	0

/**************************************************************************
PUTC - Print a character
**************************************************************************/
	.globl	putc
	.globl	_putc
putc:
_putc:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movb	8(%ebp),%cl
	call	_prot_to_real
	.code16
	movl	$1,%ebx
	movb	$0x0e,%ah
	movb	%cl,%al
	int	$0x10
	DATA32 call	_real_to_prot
	.code32
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

/**************************************************************************
MEMSIZE - Determine size of extended memory
**************************************************************************/
	.globl	memsize
memsize:
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	call	_prot_to_real
	.code16
	movw	$0xe801,%ax
	stc
	int	$0x15
	jc	1f
	andl	$0xffff,%eax
	andl	$0xffff,%ebx
	shll	$6,%ebx
	addl	%ebx,%eax
	jmp	2f
1:
	movw	$0x8800,%ax
	int	$0x15
	andl	$0xffff,%eax
2:
	movl	%eax,%esi
	DATA32 call	_real_to_prot
	.code32
	movl	%esi,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/**************************************************************************
BASEMEMSIZE - Get size of the conventional (base) memory
**************************************************************************/
	.globl	basememsize
basememsize:
	call	_prot_to_real
	.code16
	int	$0x12
	movw	%ax,%cx
	DATA32 call	_real_to_prot
	.code32
	movw	%cx,%ax
	ret

/**************************************************************************
XSTART - Transfer control to the kernel just loaded
**************************************************************************/
	.globl	xstart
xstart:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	8(%ebp),%eax
	movl	%eax,_execaddr
	movl	12(%ebp),%ebx
	movl	16(%ebp),%ecx	/* bootp record (32bit pointer) */
	addl	$28,%ecx	/* ip, udp header */
	shll	$12,%ecx
	shrw	$12,%cx
	call	_prot_to_real
	.code16
	pushl	%ecx		/* bootp record */
	pushl	%ebx		/* file header */
	movl	$((RELOC<<12)+(1f-RELOC)),%eax
	pushl	%eax
	ADDR32	LJMPI(_execaddr-_start)
1:
	addw	$8,%sp		/* XXX or is this 10 in case of a 16bit "ret" */
	DATA32 call	_real_to_prot
	.code32
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

_execaddr:
	.long	0

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
SETJMP - Non-local jump to a saved stack context
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
_REAL_TO_PROT - Go from REAL mode to Protected Mode
**************************************************************************/
	.globl	_real_to_prot
_real_to_prot:
	.code16
	cli
	cs
	ADDR32 lgdt	gdtarg-_start
	movl	%cr0,%eax
	orl	$CR0_PE,%eax
	movl	%eax,%cr0		/* turn on protected mode */

	/* flush prefetch queue, and reload %cs:%eip */
	DATA32 ljmp	$KERN_CODE_SEG,$1f
1:
	.code32
	/* reload other segment registers */
	movl	$KERN_DATA_SEG,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	addl	$RELOC,%esp		/* Fix up stack pointer */
	xorl	%eax,%eax
	movl	%eax,%fs
	movl	%eax,%gs
	popl	%eax			/* Fix up return address */
	addl	$RELOC,%eax
	pushl	%eax
	ret

/**************************************************************************
_PROT_TO_REAL - Go from Protected Mode to REAL Mode
**************************************************************************/
	.globl	_prot_to_real
_prot_to_real:
	.code32
	popl	%eax
	subl	$RELOC,%eax		/* Adjust return address */
	pushl	%eax
	subl	$RELOC,%esp		/* Adjust stack pointer */
#ifdef	GAS291
	ljmp	$REAL_CODE_SEG,$1f-RELOC	/* jump to a 16 bit segment */
#else
	ljmp	$REAL_CODE_SEG,$1f-_start	/* jump to a 16 bit segment */
#endif	/* GAS291 */
1:
	.code16
	movw	$REAL_DATA_SEG,%ax
	movw	%ax,%ds
	movw	%ax,%ss
	movw	%ax,%es
	movw	%ax,%fs
	movw	%ax,%gs

	/* clear the PE bit of CR0 */
	movl	%cr0,%eax
	andl	$0!CR0_PE,%eax
	movl	%eax,%cr0

	/* make intersegment jmp to flush the processor pipeline
	 * and reload %cs:%eip (to clear upper 16 bits of %eip).
	 */
	DATA32 ljmp	$(RELOC)>>4,$2f-_start
2:
	/* we are in real mode now
	 * set up the real mode segment registers : %ds, $ss, %es
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	sti
	DATA32 ret	/* There is a 32 bit return address on the stack */
	.code32

/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
	.align	4
_gdt:
gdtarg:
	.word	0x27			/* limit */
	.long	_gdt			/* addr */
	.byte	0,0

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

	.align	4

	.data
err_not386:
	.ascii	"Etherboot/32 requires 386+"
	.byte	0x0d, 0x0a
err_not386_end:

days:	.long	0
