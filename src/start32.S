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
	pushf				/* Save flags */
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
	lret
is_386:
	popf				/* Restore flags */
	movw	%ss,initss-_start
	movw	%sp,initsp-_start
/*
 *	Paul Robertson of Locsoft found that if the top half of %esp
 *	contains garbage, it can affect some BIOSes
 */
	andl	$0xffff,%esp
	DATA32 call	_real_to_prot
	.code32

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
	movw	initss-_start,%ss
	movw	initsp-_start,%sp
	lret

	.code32
#if defined(CONFIG_TSC_CURRTICKS)
#undef CONFIG_BIOS_CURRTICKS
#else
#define CONFIG_BIOS_CURRTICKS 1
#endif
#if defined(CONFIG_BIOS_CURRTICKS)
/**************************************************************************
CURRTICKS - Get Time
Use direct memory access to BIOS variables, longword 0040:006C (ticks
today) and byte 0040:0070 (midnight crossover flag) instead of calling
timeofday BIOS interrupt.
**************************************************************************/
	.globl	currticks
currticks:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	call	_prot_to_real
	.code16
	DATA32 call	_real_to_prot
	.code32
	movl	0x46C, %eax
	movb	0x470, %bl
	cmpb	$0, %bl
	je	notmidnite
	movb	$0, 0x470		/* clear the flag */
	addl	$0x1800b0,days		/* 0x1800b0 ticks per day */
notmidnite:
	addl	days,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
#endif	/* CONFIG_BIOS_CURRTICKS */

/**************************************************************************
CONSOLE_PUTC - Print a character on console
**************************************************************************/
	.globl	console_putc
console_putc:
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
INT10 - Call Interrupt 0x10
**************************************************************************/
#if	defined(ANSIESC) && (defined(CONSOLE_CRT) || defined(CONSOLE_DUAL) || !defined(CONSOLE_SERIAL))
	.globl	_int10
_int10:
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%esi
	push	%edi
	movw	8(%ebp),%si
	movw	10(%ebp),%bx
	movw	12(%ebp),%cx
	movw	14(%ebp),%dx
	call	_prot_to_real
	.code16
	movw	%si,%ax
	int	$0x10
	movw	%ax,%si
	DATA32 call	_real_to_prot
	.code32
	movl	%esi,%eax
	andl	$0xFFFF,%eax
	movw	%ax,int10ret
	movw	%bx,int10ret+2
	shl	$16,%ebx
	orl	%ebx,%eax
	movw	%cx,int10ret+4
	movw	%dx,int10ret+6
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret

	.globl	int10ret
int10ret:
	.word	0,0,0,0
#endif

/**************************************************************************
CONSOLE_GETC - Get a character from console
**************************************************************************/
	.globl	console_getc
console_getc:
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	call	_prot_to_real
	.code16
	movb	$0x0,%ah
	int	$0x16
	movb	%al,%bl
	DATA32 call	_real_to_prot
	.code32
	xor	%eax,%eax
	movzbl	%bl,%eax

	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/**************************************************************************
CONSOLE_ISCHAR - Check for keyboard interrupt
**************************************************************************/
	.globl	console_ischar
console_ischar:
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	call	_prot_to_real
	.code16
	xorw	%bx,%bx
	movb	$0x1,%ah
	int	$0x16
	jz	2f
	movb	%al,%bl
2:
	DATA32 call	_real_to_prot
	.code32
	movzbl	%bl,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/**************************************************************************
GETSHIFT - Get keyboard shift state
**************************************************************************/
	.globl	getshift
getshift:
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	call	_prot_to_real
	.code16
	movb	$2,%ah
	int	$0x16
	andb	$0xdf,%al
	movw	%ax,%bx
	DATA32 call	_real_to_prot
	.code32
	movzbl	%bl,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

#ifdef	POWERSAVE
/**************************************************************************
CPU_NAP - Save power by halting the CPU until the next interrupt
**************************************************************************/
	.globl	cpu_nap
cpu_nap:
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	call	_prot_to_real
	.code16
	hlt
	DATA32 call	_real_to_prot
	.code32
	popl	%edi
	popl	%esi
	popl	%ebx
	ret
#endif	/* POWERSAVE */

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
DISK_INIT - Initialize the disk system
**************************************************************************/
#ifdef	CAN_BOOT_DISK
	.globl	disk_init
disk_init:
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	call	_prot_to_real
	.code16
	xorw	%ax,%ax
	movb	$0x80,%dl
	int	$0x13
	DATA32 call	_real_to_prot
	.code32
	popl	%edi
	popl	%esi
	popl	%ebx
	ret
#endif

/**************************************************************************
DISK_READ - Read a sector from disk
**************************************************************************/
#ifdef	CAN_BOOT_DISK
	.globl	disk_read
disk_read:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movb	8(%ebp),%dl	/* drive number */
	movb	16(%ebp),%dh	/* head number */
	movb	12(%ebp),%ch	/* cylinder number */
	movb	13(%ebp),%cl	/* cylinder number */
	shl	$6,%cl
	orb	20(%ebp),%cl	/* sector number */
	movw	26(%ebp),%si
	rorw	$4,%si
	movw	24(%ebp),%bx	/* buffer */
	call	_prot_to_real
	.code16
	movw	$0x0201,%ax
	movw	%si,%es
	int	$0x13
	jc	1f
	xorw	%ax,%ax
1:
	movw	%ax,%bx
	DATA32 call	_real_to_prot
	.code32
	movzwl	%bx,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
#endif

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
	shll	$12,%ecx	/* convert to segment:offset form */
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

#ifdef	IMAGE_MULTIBOOT
/**************************************************************************
XEND - Restart Etherboot from the beginning (from protected mode)
**************************************************************************/
	.globl	xend
xend:
	cs
	lidt	idtarg_realmode-_start+RELOC
	cs
	lgdt	gdtarg-_start+RELOC
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
	 * set up the real mode segment registers : %ds, %ss, %es
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	xorl	%esp,%esp
	ADDR32 movw	initsp-RELOC,%sp

	movw	$0,%ax
	movw	%ax,%fs
	movw	%ax,%gs

	sti
	jmp	_start
	.code32
#endif	/* IMAGE_MULTIBOOT */

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
#ifdef	IMAGE_MULTIBOOT
idtarg_realmode:
	.word	0xffff			/* use reset limit */
	.long	0
	.word	0
#endif	/* IMAGE_MULTIBOOT */

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

initss:	.word	0
initsp:	.word	0
days:	.long	0

	.align	4

	.section ".rodata"
err_not386:
	.ascii	"Etherboot/32 requires 386+"
	.byte	0x0d, 0x0a
err_not386_end:
