/* #defines because ljmp wants a number, probably gas bug */
/*	.equ	KERN_CODE_SEG,_pmcs-_gdt	*/
#define	KERN_CODE_SEG	0x08
	.equ	KERN_DATA_SEG,_pmds-_gdt
/*	.equ	REAL_CODE_SEG,_rmcs-_gdt	*/
#define	REAL_CODE_SEG	0x18
	.equ	REAL_DATA_SEG,_rmds-_gdt
	.equ	FLAT_CODE_SEG,_pmcs2-_gdt
	.equ	FLAT_DATA_SEG,_pmds2-_gdt
	.equ	CR0_PE,1

#undef CODE16
#if defined(PCBIOS) || defined(TAGGED_IMAGE)
#define	CODE16
#endif
	
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
	.text
	.arch i386
	.global	_start
_start:
	.code32
	cli
	/* Save the initial ebp value */
	pushl	%ebp

	/*
	 * See where I am running, and compute virt_offset
	 */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp
	movl	%ebp, virt_offset(%ebp)

	/*
	 * Save the initial register contents
	 */
	movl	%edi,  0 + initial_regs(%ebp)
	movl	%esi,  4 + initial_regs(%ebp)
	popl	%edi /* original %ebp */
	movl	%edi,  8 + initial_regs(%ebp)
	movl	%esp, 12 + initial_regs(%ebp)
	movl	%ebx, 16 + initial_regs(%ebp)
	movl	%edx, 20 + initial_regs(%ebp)
	movl	%ecx, 24 + initial_regs(%ebp)
	movl	%eax, 28 + initial_regs(%ebp)

	/*
	 * Setup the stack
	 */
	leal	_estack(%ebp), %esp

	/* Fixup the gdt */
	leal	_pmcs(%ebp), %eax
	pushl	%eax
	pushl	%ebp
	call	set_seg_base
	addl	$8, %esp

	/* Fixup gdtarg */
	leal	_gdt(%ebp), %eax
	movl	%eax, gdtarg +2(%ebp)

	/* Load the global descriptor table */
	cs
	lgdt	gdtarg(%ebp)

	/* reload cs */
	ljmp	$KERN_CODE_SEG, $1f
1:
	/* reload other segment registers */
	movl	$KERN_DATA_SEG, %eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	movl	%eax,%fs
	movl	%eax,%gs

	/* Fixup the stack pointer */
	subl	%ebp, %esp

	call	main
	/* fall through */

	/* exit */

	/* Switch back to the original stack */
	movl	12 + initial_regs, %esp
	subl	virt_offset, %esp

#ifdef CODE16
	/* Make certain we don't have a null return address.
	 */
	cmpl	$0, 0(%esp)
	je	exit16
#endif

	/* Revert to flat physical addresses and return */
	call	_virt_to_phys
	xorl	%eax, %eax
	ret

#ifdef CODE16
exit16:
	/* We have a null return address so assume a 16bit far return
	 * address is burried deeper on the stack.
	 */
	movzwl	4(%esp), %ebx
	movzwl  6(%esp), %ecx
	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16

	cli
	/* Switch back to the callers stack, and return */
	movw	%bx, %sp
	movw	%cx, %ss
	popf
	lret
20:	.code32
	.previous
#endif /* CODE16 */

	.code32

#if defined(TAGGED_IMAGE)
/**************************************************************************
XSTART16 - Transfer control to the kernel just loaded
**************************************************************************/
	.globl	xstart16
xstart16:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	8(%ebp),%edx
	movl	12(%ebp),%ebx

	/* FIXME handle the bootp record */
	movl	16(%ebp),%ecx	/* bootp record (32bit pointer) */
	shll	$12,%ecx	/* convert to segment:offset form */
	shrw	$12,%cx

	pushl	$ 10f
	pushl	$ 20f - 10f
	call	_real_call
	.section ".text16"
10:	.code16
	popw	%ax		/* get the return ip addr */
	pushl	%ecx		/* bootp record */
	pushl	%ebx		/* file header */
	pushw	%cs		/* Setup the far return address */
	pushw	%ax		
	pushl	%edx		/* Setup the far address to call */
	lret			/* Back into the routine I'm calling */
20:	.code32
	.previous
	
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
#endif /* TAGGED_IMAGE */

#if defined(RELOCATE)
/**************************************************************************
RELOCATE_TO - relocate etherboot to the specified address
**************************************************************************/
	.globl relocate_to
relocate_to:
	/* Save the callee save registers */
	pushl	%ebp
	pushl	%esi
	pushl	%edi

	/* Compute the virtual destination address */
	movl	16(%esp), %edi	# dest
	subl	virt_offset, %edi
	

	/* Compute the new value of virt_offset */
	movl	16(%esp), %ebp	# virt_offset
	subl	$_text, %ebp

	/* Fixup the gdt */
	pushl	$_pmcs
	pushl	%ebp		# virt_offset
	call	set_seg_base
	addl	$8, %esp

	/* Fixup gdtarg */
	leal	_gdt(%ebp), %eax
	movl	%eax, gdtarg +2

	/* Fixup virt_offset */
	movl	%ebp, virt_offset

	/* Load the move parameters */
	movl	$_text, %esi
	movl	$_end, %ecx
	subl	%esi, %ecx

	/* Move etherboot uses %esi, %edi, %ecx */
	rep 
	movsb

	/* Reload the gdt */
	cs
	lgdt	gdtarg

	/* Reload %cs */
	ljmp	$KERN_CODE_SEG, $1f
1:
	/* reload other segment registers */
	movl	$KERN_DATA_SEG, %eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	movl	%eax,%fs
	movl	%eax,%gs

	/* Restore the callee save registers */
	popl	%edi
	popl	%esi
	popl	%ebp

	/* return */
	ret

#endif /* RELOCATE */
	
/**************************************************************************
XSTART32 - Transfer control to the kernel just loaded
**************************************************************************/
	.globl xstart32
xstart32:
	/* Save the callee save registers */
	movl	%ebp, os_regs + 32
	movl	%esi, os_regs + 36
	movl	%edi, os_regs + 40
	movl	%ebx, os_regs + 44

	/* save the return address */
	popl	%eax
	movl	%eax, os_regs + 48

	/* save the stack pointer */
	movl	%esp, os_regs + 52

	/* Get the new destination address */
	popl	%ecx

	/* Store the physical address of xend on the stack */
	movl	$xend32, %ebx
	addl	virt_offset, %ebx
	pushl	%ebx

	/* Store the destination address on the stack */
	pushl	$FLAT_CODE_SEG
	pushl	%ecx

	/* Switch to using physical addresses */
	call	_virt_to_phys

	/* Save the target stack pointer */
	movl	%esp, os_regs + 12(%ebp)
	leal	os_regs(%ebp), %esp

	/* Store the pointer to os_regs */
	movl	%esp, os_regs_ptr(%ebp)

	/* Load my new registers */
	popal
	movl	(-32 + 12)(%esp), %esp

	/* Jump to the new kernel
	 * The lret switches to a flat code segment
	 */
	lret

	.balign 4
	.globl xend32
xend32:
	/* Fixup %eflags */
	nop
	cli
	cld
	
	/* Load %esp with &os_regs + virt_offset */
	.byte	0xbc /* movl $0, %esp */
os_regs_ptr:
	.long	0

	/* Save the result registers */
	addl	$32, %esp
	pushal

	/* Compute virt_offset */
	movl	%esp, %ebp
	subl	$os_regs, %ebp
	
	/* Load the stack pointer */
	movl	52(%esp), %esp

	/* Enable the virtual addresses */
	leal	_phys_to_virt(%ebp), %eax
	call	*%eax

	/* Restore the callee save registers */
	movl	os_regs + 32, %ebp
	movl	os_regs + 36, %esi
	movl	os_regs + 40, %edi
	movl	os_regs + 44, %ebx
	movl	os_regs + 48, %edx
	movl	os_regs + 52, %esp

	/* Get the C return value */
	movl	os_regs + 28, %eax

	jmpl	*%edx

/**************************************************************************
SETJMP - Save stack context for non-local goto
**************************************************************************/
	.globl	setjmp
setjmp:
	movl	4(%esp),%ecx		/* jmpbuf */
	movl	0(%esp),%edx		/* return address */
	movl	%edx,0(%ecx)
	movl	%ebx,4(%ecx)
	movl	%esp,8(%ecx)
	movl	%ebp,12(%ecx)
	movl	%esi,16(%ecx)
	movl	%edi,20(%ecx)
	movl	$0,%eax
	ret

/**************************************************************************
LONGJMP - Non-local jump to a saved stack context
**************************************************************************/
	.globl	longjmp
longjmp:
	movl	4(%esp),%edx		/* jumpbuf */
	movl	8(%esp),%eax		/* result */
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
_VIRT_TO_PHYS - Transition from virtual to physical addresses
**************************************************************************/
	.globl _virt_to_phys
_virt_to_phys:
	movl	virt_offset, %ebp	/* Load virt_offset */
	addl	%ebp, 0(%esp)		/* Adjust the return address */

	/* reload the code segment */
	pushl	$FLAT_CODE_SEG
	leal	1f(%ebp), %eax
	pushl	%eax
	lret

1:
	/* reload other segment registers */
	movl	$FLAT_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es	
	movl	%eax, %ss	
	movl	%eax, %fs	
	movl	%eax, %gs

	/* Adjust the stack pointer, after we have reloaded the stack segment */
	addl	%ebp, %esp		

	ret


/**************************************************************************
_PHYS_TO_VIRT - Transition from using physical to virtual addresses
**************************************************************************/
	.globl _phys_to_virt
_phys_to_virt:
	/* virt_offset is in %ebp */
	subl	%ebp, 0(%esp)	/* Adjust the return address */
	subl	%ebp, %esp	/* Adjust the stack pointer */

	ljmp	$KERN_CODE_SEG, $1f
1:
	/* reload other segment regsters */
	movl	$KERN_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es	
	movl	%eax, %ss	
	movl	%eax, %fs	
	movl	%eax, %gs	
	ret
	

/**************************************************************************
SET_SEG_BASE - Set the base address of a segment register
**************************************************************************/
	.globl set_seg_base
set_seg_base:
	/* Low half of the gdt base */
	movl	4(%esp), %eax
	shll	$16, %eax

	/* High half of the gdt base */	
	movl	4(%esp), %ecx
	shrl	$16, %ecx
	andl	$0xff, %ecx

	movl	4(%esp), %edx
	andl	$0xff000000, %edx
	orl	%edx, %ecx

	movl	8(%esp), %edx

	/* Fixup the code segment */
	andl	$0x0000ffff,  0(%edx)
	orl	%eax       ,  0(%edx)
	andl	$0x00ffff00,  4(%edx)
	orl	%ecx       ,  4(%edx)

	/* Fixup the data segment */
	andl	$0x0000ffff,  8(%edx)
	orl	%eax       ,  8(%edx)
	andl	$0x00ffff00, 12(%edx)
	orl	%ecx       , 12(%edx)

	ret

#ifdef CODE16
/**************************************************************************
_REAL_CALL - Run some code in real mode.
**************************************************************************/
	/* MAX_REAL_MODE_STACK is carefully tuned to work
	 * with the stack bottom at 0x7c00 while not chancing
	 * overwriting data below 0x500.
	 */
#define MAX_REAL_MODE_STACK 29696
#define RADDR(sym)	(((sym) - _end16) + MAX_REAL_MODE_STACK)

	.balign 4
	.globl real_mode_stack
real_mode_stack:
	.long 0x7c00  /* Put the stack just below the dos load address */
real_stack_top:
	.long 0
_save_esp:
	.long 0

	.globl _real_call
_real_call:
	/* Save the original %esp value */
	movl	%esp, _save_esp
	
	/* Save the temporary registers I use */
	pushl	$0
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	
	/* Load up the registers */
	movl	32(%esp), %ecx		/* The 16bit code len */
	movl	36(%esp), %esi		/* The 16bit code start */
	movl	virt_offset, %ebp	/* The virtual offset */
	
	/* stack top = phys_to_virt(real_mode_stack - MAX_REAL_MODE_STACK) */
	movl	real_mode_stack, %ebx	/* The stack top */
	subl	$MAX_REAL_MODE_STACK, %ebx
	movl	%ebx, real_stack_top
	subl	%ebp, %ebx

	/* Save the real mode stack top */
	movl	%ebx, 24(%esp)

	/* Compute where the copied code goes */
	leal	RADDR(__real_call)(%ebx), %edi
	subl	%ecx, %edi
	andl	$0xfffffffc, %edi	/* 4 byte aligned */

	/* Remember where the code is executed */
	movl	%edi, %eax
	subl	%ebx, %eax
	movw	%ax, real_ip

	/* Copy the user code onto the real mode stack */
	rep
	movsb
	
	/* Copy the trampoline onto the stack */
	movl	$__real_call, %esi
	movl	$_end16 - __real_call, %ecx
	leal	RADDR(__real_call)(%ebx), %edi
	rep
	movsb

	/* Fixup real_gdtarg */
	leal	_gdt(%ebp), %eax
	movl	%eax, RADDR(real_gdtarg +2)(%ebx)
	
	/* Fixup the gdt */
	pushl	$_rmcs
	leal	0(%ebx, %ebp), %eax
	pushl	%eax
	call	set_seg_base
	addl	$8, %esp

	/* Restore the saved registers */
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx

	/* And switch stacks */
	popl	%esp
	movzwl	RADDR(real_ip)(%esp), %eax
	addl	%eax, %esp

	/* Setup for jump to real mode */
	movl	real_stack_top, %eax
	shrl	$4, %eax
	pushw	%ax
	pushw	$RADDR(real16)

	/* Switch stack from %esp 32bit virtual to %sp 16bit physical */
	addl	virt_offset, %esp
	subl	real_stack_top, %esp

	/* Jump to 16bit code */
	ljmp	$REAL_CODE_SEG, $RADDR(code16) 	/* jump to a 16 bit segment */
_real_call_ret:
	/* reload  segment registers */
	movl	$KERN_DATA_SEG,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	movl	%eax,%fs
	movl	%eax,%gs

	/* Restore the stack */
	movl	_save_esp, %esp

	/* Restore the direction flag */
	cld

	/* Get the real mode stack pointer */
	movl	real_stack_top, %eax
	subl	virt_offset, %eax
	pushl	%eax
	movzwl	RADDR(real_sp)(%eax), %eax
	addl	0(%esp), %eax
	addl	$4, %esp
	
	/* Return to my caller */
	ret	$8

	
	.balign 16
__real_call:
real_sp:
	.word 0
real_ip:
	.word 0
real_gdtarg:
	.word	_gdt_end - _gdt - 1	/* limit */
	.long	_gdt			/* addr */
	.code16
code16:
	/* Load 16bit segment descriptors to force 16bit segment limit */
	movw	$REAL_DATA_SEG, %ax
	movw	%ax,%ds
	movw	%ax,%ss
	movw	%ax,%es
	movw	%ax,%fs
	movw	%ax,%gs

	/* clear the PE bit of CR0 */
	movl	%cr0,%eax
	andb	$0!CR0_PE,%al
	movl	%eax,%cr0

	/* make intersegment jmp to flush the processor pipeline
	 * and reload %cs:%eip (to clear upper 16 bits of %eip).
	 */
	lret
real16:	
	/* we are in real mode now
	 * set up the real mode segment registers : %ds, $ss, %es
	 */
	movw	%cs, %ax
	movw	%ax, %ss
	movw	%ax, %ds
	movw	%ax, %fs
	movw	%ax, %gs

	/* Enable interrupts */
	sti

	/* Call the user supplied code */
	call	*RADDR(real_ip)

	/* Save the stack pointer */
	cs
	movw	%sp, RADDR(real_sp)

	/* Disable interrupts */
	cli

	/* Switch back to protected mode */
	cs
	DATA32 lgdt RADDR(real_gdtarg)
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0	/* turn on protected mode */

	/* flush prefetch queue, and reload %cs:%eip */
	DATA32 ljmp	$KERN_CODE_SEG, $_real_call_ret
	.code32
__end16:
	.balign 16
_end16:
	.code32

#endif /* CODE16 */

/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
	.align	4

_gdt:
gdtarg:
	.word	_gdt_end - _gdt - 1	/* limit */
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
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x9b,0x00,(0>>24)

_rmds:
	/* 16 bit real mode data segment */
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x93,0x00,(0>>24)

_pmcs2:
	/* 32 bit protected mode code segment, base 0 */
	.word	0xffff,0
	.byte	0,0x9f,0xcf,0

_pmds2:
	/* 32 bit protected mode data segment, base 0 */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0

_gdt_end:

	/* The initial register contents */
	.balign 4
	.globl initial_regs
initial_regs:
	.fill 8, 4, 0

	/* The virtual address offset  */	
	.globl virt_offset
virt_offset:
	.long  	0

	.section ".bss"
	.p2align 3
	/* allocate a 4K stack in the bss segment */
_stack:
	.space 4096
_estack:
	