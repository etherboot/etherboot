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
#ifdef CONFIG_X86_64
	.equ	LM_CODE_SEG,  _lmcs-_gdt
	.equ	LM_DATA_SEG,  _lmds-_gdt
#endif

	.equ	MSR_K6_EFER,   0xC0000080
	.equ	EFER_LME,      0x00000100
	.equ	X86_CR4_PAE,   0x00000020
	.equ	CR0_PG,        0x80000000

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

#include "callbacks.h"
#define NUM_PUSHA_REGS (8)
#define NUM_SEG_REGS (6)
	
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
	.code32
_start:
	/* We use _in_call to do the actual work.
	 */
	pushl	$(EB_OPCODE_MAIN | EB_USE_INTERNAL_STACK)
#ifdef CODE16
	/* Check for entry via start16 (indicated by a null return
	 * address) and set the flag EB_CALL_FROM_REAL_MODE if so.
	 */
	cmpl	$0, 4(%esp)
	orl	$EB_CALL_FROM_REAL_MODE, 0(%esp)
#endif
	call	_in_call
	addl	$4, %esp			/* skip opcode */
	
	/* Addresses have reverted to physical by this point.  Note
	 * that if we entered via start16, we will never reach this
	 * point; an exit interceptor is used to return to the prefix.
	 */	

	/* Return to caller.  Exit code is exit code from main(). */
	ret

/**************************************************************************
_IN_CALL - make a call in to Etherboot.
**************************************************************************/

/* There are two 32-bit entry points: _in_call and _in_call_far, for
 * near calls and far calls respectively.  Both should be called with
 * flat physical addresses.  They will result in a call to the C
 * routine in_call(); see there for API details.
 *
 * Note that this routine makes fairly heavy use of the stack and no
 * use of fixed data areas.  This is because it must be re-entrant;
 * there may be more than one concurrent call in to Etherboot.
 */

#define IC_OFFSET_VA_LIST_PTR ( 0 )
#define IC_OFFSET_VA_LIST_PTR_E ( IC_OFFSET_VA_LIST_PTR + 4 )
#define IC_OFFSET_REGISTERS ( IC_OFFSET_VA_LIST_PTR_E )
#define IC_OFFSET_REGISTERS_E ( IC_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
#define IC_OFFSET_SEG_REGS ( IC_OFFSET_REGISTERS_E )
#define IC_OFFSET_SEG_REGS_E ( IC_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define IC_OFFSET_GDT ( IC_OFFSET_SEG_REGS_E )
#define IC_OFFSET_GDT_E ( IC_OFFSET_GDT + 8 )
#define IC_OFFSET_FLAGS ( IC_OFFSET_GDT_E )
#define IC_OFFSET_FLAGS_E ( IC_OFFSET_FLAGS + 4 )
#define IC_OFFSET_RETADDR ( IC_OFFSET_FLAGS_E )
#define IC_OFFSET_RETADDR_E ( IC_OFFSET_RETADDR + 8 )
#define IC_OFFSET_ORIG_STACK ( IC_OFFSET_RETADDR )
#define IC_OFFSET_OPCODE ( IC_OFFSET_ORIG_STACK + 8 )
#define IC_OFFSET_OPCODE_E ( IC_OFFSET_OPCODE + 4 )
#define IC_OFFSET_VA_LIST ( IC_OFFSET_OPCODE_E )
	
	.code32
	.globl _in_call
	.globl _in_call_far
_in_call:
	pushl	$0			/* Indicate near return address */
_in_call_far:
	/* Store flags */
	pushfl
	/* Store the GDT */
	subl	$8, %esp
	sgdt	0(%esp)
	/* Store segment register values */
	pushw	%cs
	pushw	%ss
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs
	/* Store general-purpose register values */
	pushal
	/* Replace %esp in store with physical %esp value on entry */
	leal	(IC_OFFSET_ORIG_STACK - IC_OFFSET_REGISTERS)(%esp), %eax
	movl	%eax, (IC_OFFSET_REGISTERS - IC_OFFSET_REGISTERS + 12)(%esp)
	/* Store va_list pointer (physical address) */
	leal	(IC_OFFSET_VA_LIST - IC_OFFSET_VA_LIST_PTR_E)(%esp), %eax
	pushl	%eax
	/* IC_OFFSET_*(%esp) are now valid */

	/*
	 * See where I am running, and compute virt_offset.  Yes,
	 * we're overwriting a global variable, but if the virt_offset
	 * really has changed since the last call then we're in
	 * trouble already.
	 */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp
	movl	%ebp, virt_offset(%ebp)

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
	cli				/* Temporarily disable interrupts */
	lgdt	%cs:gdtarg(%ebp)

	/* reload cs */
	ljmp	$KERN_CODE_SEG, $1f
1:
	/* reload other segment registers */
	movl	$KERN_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs

	/* Fixup the stack pointer */
	subl	%ebp, %esp

	/* Fixup the va_list pointer */
	subl	%ebp, IC_OFFSET_VA_LIST_PTR(%esp)

	/* Check opcode for EB_USE_INTERNAL_STACK flag */
	movl	IC_OFFSET_OPCODE(%esp), %eax
	testl	$EB_USE_INTERNAL_STACK, %eax
	je	2f
	/* Use internal stack flag set */
	/* Check %esp is not already in internal stack range */
	leal	_stack, %esi		/* %esi = bottom of internal stack */
	leal	_estack, %edi		/* %edi = top of internal stack */
	cmpl	%esi, %esp
	jb	1f
	cmpl	%edi, %esp
	jbe	2f
1:	/* %esp not currently in internal stack range */
	movl	%esp, %esi		/* %esi = original stack */
	movl	$IC_OFFSET_OPCODE_E, %ecx /* %ecx = length to transfer */
	subl	%ecx, %edi		/* %edi = internal stack pos */
	movl	%edi, %esp		/*  = new %esp */
	rep movsb			/* Copy data to internal stack */
2:

	/* Call to C code */
	call	i386_in_call

	/* Set %eax (return code from C) in registers structure on
	 * stack, so that we return it to the caller.
	 */
	movl	%eax, (IC_OFFSET_REGISTERS + 28)(%esp)

	/* Calculate physical continuation address */
	movl	virt_offset, %ebp
	movzwl	(IC_OFFSET_SEG_REGS + 10)(%esp), %eax	/* %cs */
	movzwl	(IC_OFFSET_SEG_REGS + 8)(%esp), %ebx	/* %ss */
	pushl	%eax			/* Continuation segment */
	leal	1f(%ebp), %eax
	pushl	%eax			/* Continuation offset */
	
	/* Restore caller's GDT */
	cli				/* Temporarily disable interrupts */
	lgdt	(8+IC_OFFSET_GDT)(%esp)
	/* Reset %ss and adjust %esp */
	movw	%bx, %ss
	addl	%ebp, %esp
	lret				/* Reload %cs:eip, flush prefetch */
1:

	/* Skip va_list ptr */
	popl	%eax
	/* Reload general-purpose registers to be returned */
	popal
	/* Reload segment registers as passed in from caller */
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	addl	$(4+8), %esp	/* Skip %cs, %ss and GDT (already reloaded) */
	/* Restore flags (including revert of interrupt status) */
	popfl

	/* Restore physical %esp from entry.  It will only be
	 * different if EB_USE_INTERNAL_STACK was specified.
	 */
	movl	( 12 + IC_OFFSET_REGISTERS - IC_OFFSET_RETADDR )(%esp), %esp
		
	cmpl	$0, 0(%esp)		/* Check for near/far return */
	je	2f
1:	/* far return */
	lret
2:	/* near return */
	addl	$4, %esp
	ret

/**************************************************************************
EXT_CALL - make a call out of Etherboot.
**************************************************************************/
	
/* Call to an external routine using flat physical addresses.  Has the
 * capability to pass in and out register and stack contents, relocate
 * stack to a new address, use a trampoline routine on the stack etc.
 *
 * Note that this routine makes fairly heavy use of the stack and no
 * use of fixed data areas.  This is because it must be re-entrant;
 * there may be more than one concurrent call out of Etherboot.
 */

#define EC_SIGNATURE ( 0x43747845 ) /* "ExtC" */

/* Offsets from base of signature block */
#define EC_OFFSET_SIGBLOCK (0)
#define EC_SIG_SIG1 (0)
#define EC_SIG_LENGTH ( EC_SIG_SIG1 + 4 )
#define EC_SIG_STACK ( EC_SIG_LENGTH + 4 )
#define EC_SIG_TRAMPOLINE ( EC_SIG_STACK + 4 )
#define EC_SIG_SIG2 ( EC_SIG_TRAMPOLINE + 4 )
#define EC_SIGBLOCK_LENGTH ( EC_SIG_SIG2 + 4 )
#define EC_SIG_TRAMPOLINE_RET ( EC_SIG_SIG1 - 4 )
#define EC_SIG_PARAMS_TOP ( EC_SIG_TRAMPOLINE_RET )
#define EC_OFFSET_CALLPRESERVE ( EC_OFFSET_SIGBLOCK + EC_SIGBLOCK_LENGTH )
#define EC_CALLPRESERVE_LENGTH (16)
#define EC_OFFSET_RETADDR ( EC_OFFSET_CALLPRESERVE + EC_CALLPRESERVE_LENGTH )
#define EC_RETADDR_LENGTH (4)
#define EC_OFFSET_ARGUMENTS ( EC_OFFSET_RETADDR + EC_RETADDR_LENGTH )
#define EC_OFFSET_CALLADDR ( EC_OFFSET_ARGUMENTS )
#define EC_OFFSET_CALLADDR_E ( EC_OFFSET_ARGUMENTS + 4 )
#define EC_OFFSET_ARG_LIST ( EC_OFFSET_CALLADDR_E )

/* Offsets from current %esp */
/* Outgoing structure */
#define EC_OFFSET_PREVIOUS_VA_LIST 0
#define EC_OFFSET_FLAGS ( EC_OFFSET_PREVIOUS_VA_LIST + 4 )
#define EC_OFFSET_REGISTERS ( EC_OFFSET_FLAGS + 4 )
#define EC_OFFSET_ADDRESSES ( EC_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
#define EC_OFFSET_PARAMS ( EC_OFFSET_ADDRESSES + 12 )
/* Return structure */
#define ECR_OFFSET_PREVIOUS_VA_LIST 0
#define ECR_OFFSET_FLAGS ( ECR_OFFSET_PREVIOUS_VA_LIST + 4 )
#define ECR_OFFSET_REGISTERS ( ECR_OFFSET_FLAGS + 4 )
#define ECR_OFFSET_PARAMS ( ECR_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
/* Signature block parameters that are reused after return */
#define ECR_SIG_RET_LENGTH ( EC_SIG_TRAMPOLINE )

	.globl _v_ext_call
_v_ext_call:
	/* Save registers required to return to C code */
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	/* We set up a stack that looks like this:
	 * 
	 * EC_SIGNATURE		%ebp + EC_SIG_SIG2
	 * trampoline		%ebp + EC_SIG_TRAMPOLINE
	 * stack_ptr		%ebp + EC_SIG_STACK
	 * length		%ebp + EC_SIG_LENGTH
	 * EC_SIGNATURE		%ebp + EC_SIG_SIG1 ( = %ebp + 0 )
	 * trampoline_return	%ebp + EC_SIG_TRAMPOLINE_RET ( = %ebp - 8 )
	 * trampoline		%esp + EC_OFFSET_PARAMS + [EC_SIG_TRAMPOLINE]
	 * ext_params		%esp + EC_OFFSET_PARAMS
	 * addresses		%esp + EC_OFFSET_ADDRESSES
	 * registers		%esp + EC_OFFSET_REGISTERS
	 * flags		%esp + EC_OFFSET_FLAGS
	 * previous_va_list	%esp + EC_OFFSET_PREVIOUS_VA_LIST ( = %esp+0 )
	 *
	 * trampoline : offset into ext_params of the trampoline code,
	 * if any.  This is used only if the target address for the
	 * ext_call is zero.
	 *
	 * stack_ptr : used to record the address to which the stack
	 * should be copied before jumping to the external routine.
	 * (On return, used to record the original stack address to
	 * which the stack should be copied back).  Address is the top
	 * of the stack (i.e. %ebp + EC_SIGBLOCK_LENGTH).
	 *
	 * trampoline_return : 4 bytes reserved at the end of
	 * ext_params.  The return address is stored at the end of
	 * ext_params when a trampoline is used, enabling the
	 * trampoline to discard the return address at the head of the
	 * stack if necessary.  Note that the return address is
	 * aligned to follow immediately from the end of the
	 * trampoline code.
	 *
	 * trampoline : Trampoline code to be executed.  If the call
	 * address is zero, the address of the trampoline code (after
	 * stack relocation) will be used instead.
	 *
	 * ext_params : Parameters to pass on stack to external call.
	 * "length" indicates the length of this data, which may not
	 * be a multiple of 4.  The bottom of ext_params is kept
	 * aligned, i.e. there is padding at the top.
	 *
	 * addresses : return and call addresses.  Always 3 dwords.  A
	 * near return address and a far call address (to allow for a
	 * change to flat physical addresses).
	 *
	 * registers : registers to set up before calling to external
	 * routine.  8 dwords, suitable for a single popal
	 * instruction.
	 *
	 * previous_va_list : a temporary pointer used to enable
	 * va_lists to contain further va_lists.
	 *
	 * %ebp is kept pointing to the bottom EC_SIGNATURE.
	 *
	 * %ebx is used to point to the current argument in our
	 * argument list as we process them.
	 */

	/* Create signature block */
	movl	$EC_SIGNATURE, %eax
	xorl	%ecx, %ecx
	movl	%esp, %ebx
	addl	virt_offset, %ebx
	pushl	%eax			/* Signature */
	pushl	%ecx			/* trampoline */
	pushl	%ebx			/* stack_ptr (physical) */
	pushl	%ecx			/* length */
	pushl	%eax			/* Signature */
	movl	%esp, %ebp		/* and set %ebp */

	/* Store values in trampoline_return */
	leal	ec_return, %eax
	addl	virt_offset, %eax	/* %eax = physical return address */
	pushl	%eax

	/* Set up return and call addresses */
	pushl	%eax			/* Near return address */
	pushl	$FLAT_CODE_SEG		/* Far call address */
	pushl	EC_OFFSET_CALLADDR(%ebp)

	/* Save registers.  (Current register contents are fairly
	 * meaningless, but using pushal is the most efficient way to
	 * allocate the stack space.)
	 */
	pushal

	/* Save flags */
	pushfl

	/* Initially there is no previous va_list */
	xorl	%eax, %eax
	pushl	%eax

	/* Set %ebx to point to first argument in list */
	movl	EC_OFFSET_ARG_LIST(%ebp), %ebx
	
	/* Ensure that movsb etc work as expected */
	cld

	/* Loop to process parameters to _ext_call */
process_ec_params:
	movl	0(%ebx), %eax		/* %eax = type */
	addl	$4, %ebx
	cmpl	$EXTCALL_END_LIST, %eax
	je	process_ec_params_end_list
	cmpl	$EXTCALL_NONE, %eax
	je	process_ec_params
	cmpl	$EXTCALL_REGISTERS, %eax
	je	process_ec_params_registers
	cmpl	$EXTCALL_STACK, %eax
	je	process_ec_params_stack_or_trampoline
	cmpl	$EXTCALL_RET_STACK, %eax
	je	process_ec_params_ret_stack
	cmpl	$EXTCALL_RELOC_STACK, %eax
	je	process_ec_params_reloc_stack
	cmpl	$EXTCALL_TRAMPOLINE, %eax
	je	process_ec_params_stack_or_trampoline
	cmpl	$EXTCALL_VA_LIST, %eax
	je	process_ec_params_va_list
	/* fall through to error */
process_ec_params_error:
	/* Return -1 to indicate an error */
	xorl	%eax, %eax
	decl	%eax
	jmp	ec_return_to_etherboot
process_ec_params_registers:
	/* Copy registers from C structure to our register store on
	 * stack (i.e. replace the contents of the pushal that we
	 * issued.)
	 */
	movl	0(%ebx), %esi		/* %esi = &registers */
	addl	$4, %ebx
	movl	$NUM_PUSHA_REGS, %ecx
	leal	EC_OFFSET_REGISTERS(%esp), %edi
	rep movsl
	jmp	process_ec_params
process_ec_params_stack_or_trampoline:
	/* Move data currently on stack down to accommodate new data */
	/* Two possible insertion points: at end or before trampoline */
	movl	EC_SIG_LENGTH(%ebp), %ecx /* %ecx = current length */
	movl	%ecx, %edx		  /* insert at end */
	cmpl	$EXTCALL_TRAMPOLINE, %eax
	je	1f
	/* Not trampoline: insert before trampoline, update trampoline offset*/
	movl	EC_SIG_TRAMPOLINE(%ebp), %edx /* %edx = trampoline offset */
	movl	4(%ebx), %eax
	addl	%eax, EC_SIG_TRAMPOLINE(%ebp) /* update trampoline offset */
1:	/* 0(%ebx) = address of data to insert */
	/* 4(%ebx) = length of data to insert */
	/* %ecx = current length */
	/* %edx = offset within stack to insert */
	negl	%edx
	addl	%ecx, %edx		/* %edx = post-insertion length */
	movl	4(%ebx), %eax		/* %eax = insertion length */
	addl	$(EC_OFFSET_PARAMS), %ecx /* %ecx = data len to copy */
	movl	%esp, %esi		/* Source location */
	leal	EC_SIG_PARAMS_TOP(%ebp), %edi
	subl	%ecx, %edi
	subl	%eax, %edi
	andl	$~0x3, %edi		/* Destination location */
	movl	%edi, %esp		/* = new stack location */
	/* Move entire stack down */
	rep movsb			/* Shift stack down to make room */
	/* Move post-insertion data back up (backwards copy) */
	decl	%edi
	movl	%edi, %esi
	addl	%eax, %edi
	movl	%edx, %ecx
	std
	rep movsb
	cld
	/* Insert data */
	incl	%edi
	subl	%eax, %edi
	movl	%eax, %ecx		/* Extra data length */
	movl	0(%ebx), %esi		/* Extra data source */
	rep movsb			/* Copy in new data */
	addl	%eax, EC_SIG_LENGTH(%ebp) /* Update length */
	addl	$8, %ebx
	jmp	process_ec_params	
process_ec_params_ret_stack:
	/* Nothing to do */
	addl	$12, %ebx
	jmp	process_ec_params
process_ec_params_reloc_stack:
	/* Set stack relocation address */
	movl	0(%ebx), %eax		/* %eax = &(new stack top) */
	andl	$~0x3, %eax		/* ...round down to align */
	movl	%eax, EC_SIG_STACK(%ebp) /* ...and store it */
	addl	$4, %ebx
	jmp	process_ec_params
process_ec_params_va_list:
	/* Record current "previous list" pointer as return point for
	 * this new list.
	 */
	movl	EC_OFFSET_PREVIOUS_VA_LIST(%esp), %eax
	movl	%eax, 4(%ebx)
	/* Record current position as "previous list" pointer */
	movl	%ebx, EC_OFFSET_PREVIOUS_VA_LIST(%esp)
	/* Start processing new list */
	movl	0(%ebx), %ebx
	jmp	process_ec_params
process_ec_params_end_list:
	/* See if we should return to a previous list */
	movl	EC_OFFSET_PREVIOUS_VA_LIST(%esp), %ebx
	testl	%ebx, %ebx
	je	process_ec_params_done
	/* Restore "previous list" pointer from the previous list */
	movl	4(%ebx), %eax
	movl	%eax, EC_OFFSET_PREVIOUS_VA_LIST(%esp)
	addl	$8, %ebx
	jmp	process_ec_params
process_ec_params_done:
	/* Stack is all set up as described above.  Copy to new
	 * location, set up the register values and jump to the
	 * function.
	 */

	/* Switch to flat physical stack segment, adjust %esp and
	 * %ebp, reload segment registers
	 */
	movl	$FLAT_DATA_SEG, %eax
	movl	virt_offset, %ebx
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss
	addl	%ebx, %esp
	addl	%ebx, %ebp
	
	/* Copy stack to new location indicated by stack_ptr, reset
	 * %ss:esp to point to it.
	 */
	movl	%esp, %esi		/* %esi = &(original stack) */
	leal	EC_SIGBLOCK_LENGTH(%ebp), %edi /* %edi = &(orig. stack top) */
	movl	%edi, %ecx
	subl	%esp, %ecx		/* %ecx = length */
	xchgl	EC_SIG_STACK(%ebp), %edi /* %es:edi = &(new stack top),	   */
					 /* stack_ptr = &(orig. stack top) */
	subl	%ecx, %edi		/* %es:edi = &(new stack) */
	movl	%edi, %esp		/* Set new %esp */
	rep movsb			/* Copy stack */
	leal	(-EC_SIGBLOCK_LENGTH)(%edi), %ebp /* Set new %ebp */

	/* Check for use of trampoline, edit call address as
	 * appropriate.
	 */
	movl	(EC_OFFSET_ADDRESSES+0)(%esp), %eax
	testl	%eax, %eax
	jne	1f			/* No trampoline */
	/* Align trampoline return address to end of trampoline code */
	movl	(EC_OFFSET_ADDRESSES+8)(%esp), %ecx
	movl	EC_SIG_LENGTH(%ebp), %edx
	movl	%ecx, EC_OFFSET_PARAMS(%esp,%edx)
	/* Set trampoline address as call address */
	movl	EC_SIG_TRAMPOLINE(%ebp), %eax /* %eax = (trampoline-params) */
	addl	$EC_OFFSET_PARAMS, %eax	/* %eax = (trampoline-%esp) */
	addl	%esp, %eax		/* %eax = trampoline address */
	movl	%eax, (EC_OFFSET_ADDRESSES+0)(%esp)
1:	
	/* Skip previous va_list pointer */
	popl	%eax
	
	/* Restore flags. */
	popfl
	
	/* Restore general-purpose registers */
	popal

	/* Call to routine and switch to flat physical %cs */
	lret

	/* Return point */
ec_return:
	/* This must be position-independent code assuming nothing
	 * about the segment registers etc. until we've reloaded our
	 * GDT.  We can only rely on %cs and %ss.
	 *
	 * Basic strategy is to set up a structure similar to the one
	 * we had before the call, only with the returned values
	 * instead of the outward values.  We omit the "addresses"
	 * section.
	 */

	/* Record general-purpose registers */
	pushal

	/* Record flags */
	pushfl
	
	/* Initially there is no previous va_list */
	xorl	%eax, %eax
	pushl	%eax

	/* Point ds and es to the stack, for ease of signature
	 * scanning and re-aligning.
	 */
	pushw	%ss
	popw	%ds
	pushw	%ss
	popw	%es
	cld			/* Just in case */
	
	/* Locate signature.  It is guaranteed to be on a dword
	 * boundary, but %esp may not be.
	 */
	movl	$EC_SIGNATURE, %eax
	leal	ECR_OFFSET_PARAMS(%esp), %edi /* Returned %esp */
	andl	$~0x3, %edi	/* Round down to align */
	xorl	%ecx, %ecx
	decl	%ecx		/* Scan indefinitely */
1:	repne scasl		/* Search for signature */
	cmpl	(EC_SIG_SIG2-4)(%edi), %eax /* Check second signature */
	jne	1b		/* Continue searching if not found */
	subl	$4, %edi
	movl	%edi, %ebp
	/% ebp now points to signature, as before call */

	/* Calculate length of returned parameters and store */
	movl	EC_SIG_TRAMPOLINE(%ebp), %ecx /* Trampoline length */
	testl	%ecx, %ecx
	jnz	2f
1:	/* No trampoline: calculate length */
	leal	EC_SIG_PARAMS_TOP(%ebp), %ecx	/* &(signature) */
	leal	ECR_OFFSET_PARAMS(%esp), %edx /* Returned %esp */
	subl	%edx, %ecx
2:	/* Trampoline present: length is just the trampoline offset */
	movl	%ecx, EC_SIG_LENGTH(%ebp) /* Store length */
	xorl	%eax, %eax
	movl	%eax, ECR_SIG_RET_LENGTH(%ebp) /* Zero returned length */

	/* Align stack by shifting down */
	addl	$ECR_OFFSET_PARAMS, %ecx /* Length to copy */
	movl	%esp, %esi
	andl	$~0x3, %esp
	movl	%esp, %edi
	rep movsb		/* Shift stack down */
	/* Stack aligned, length set as before, structures in place */

	/* Reload GDT, copy back stack and reset segment registers. */
	cli			/* Disable interrupts */
	call	1f
1:	popl	%eax
	subl	$1b, %eax
	lgdt	%cs:gdtarg(%eax) /* Reload our standard GDT */
	ljmp	$KERN_CODE_SEG, $1f /* Reset our %cs */
1:	movw	$KERN_DATA_SEG, %ax
	/* Leave %ss pointing to old (external routine's) stack segment */
	movw	%ax, %ds	/* Reload other segment registers */
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	/* Transfer stack contents back to Etherboot stack, reload
	 * %ss:esp and %ebp
	 */
	movl	%esp, %esi	/* %ss:esi = source address */
	leal	EC_SIGBLOCK_LENGTH(%ebp), %ecx
	subl	%esp, %ecx	/* %ecx = stack length */
	movl	EC_SIG_STACK(%ebp), %edi /* %edi = phys. stack top */
	subl	virt_offset, %edi /* %edi = virt. stack stop */
	movl	%edi, %ebp
	subl	%ecx, %edi	/* %es:edi = dest address */
	subl	$EC_SIGBLOCK_LENGTH, %ebp /* New %ebp */
	movl	%edi, %esp	/* New %esp */
	rep ss movsb
	movw	%ax, %ss	/* Reload %ss */

	/* %ebx now points to first parameter in list, as before */
	movl	EC_OFFSET_ARG_LIST(%ebp), %ebx

	/* Reprocess argument list */
process_ecr_params:
	movl	0(%ebx), %eax		/* %eax = type */
	addl	$4, %ebx
	cmpl	$EXTCALL_END_LIST, %eax
	je	process_ecr_params_end_list
	cmpl	$EXTCALL_NONE, %eax
	je	process_ecr_params
	cmpl	$EXTCALL_REGISTERS, %eax
	je	process_ecr_params_registers
	cmpl	$EXTCALL_STACK, %eax
	je	process_ecr_params_stack_or_trampoline
	cmpl	$EXTCALL_RET_STACK, %eax
	je	process_ecr_params_ret_stack
	cmpl	$EXTCALL_RELOC_STACK, %eax
	je	process_ecr_params_reloc_stack
	cmpl	$EXTCALL_TRAMPOLINE, %eax
	je	process_ecr_params_stack_or_trampoline
	cmpl	$EXTCALL_VA_LIST, %eax
	je	process_ecr_params_va_list
	/* fall through to error */
process_ecr_params_error:
	/* Return -1 to indicate an error */
	xorl	%eax, %eax
	decl	%eax
	jmp	ec_return_to_etherboot
process_ecr_params_registers:
	/* Copy registers from register store on stack to C structure */
	movl	0(%ebx), %edi		/* %edi = &registers */
	addl	$4, %ebx
	movl	$NUM_PUSHA_REGS, %ecx
	leal	ECR_OFFSET_REGISTERS(%esp), %esi
	rep movsl
	jmp	process_ecr_params
process_ecr_params_stack_or_trampoline:
	/* Nothing to do */
	addl	$8, %ebx
	jmp	process_ecr_params
process_ecr_params_ret_stack: 
	/* Copy returned stack to C structure.  Be careful of maximum
	 * structure length
	 */
	movl	EC_SIG_LENGTH(%ebp), %ecx /* %ecx = returned stack length */
	movl	ECR_SIG_RET_LENGTH(%ebp), %eax /* %eax = returned len so far */
	leal	ECR_OFFSET_PARAMS(%esp), %esi /* %esi = &(returned stack) */
	addl	%eax, %esi		/* %esi = &(unreturned stack portion)*/
	subl	%eax, %ecx		/* %ecx = unreturned stack length */
	movl	0(%ebx), %edi		/* %edi = &(C structure) */
	movl	8(%ebx), %eax		/* %eax = &(C length variable) */
	testl	%eax, %eax
	je	1f
	movl	%ecx, (%eax)		/* Store length if applicable */
1:	movl	4(%ebx), %eax		/* %eax = max length allowed */
	cmpl	%eax, %ecx		/* Check length against max length */
	jbe	2f
	movl	%eax, %ecx		/* ...and truncate if necessary */
2:	addl	%ecx, ECR_SIG_RET_LENGTH(%ebp) /* Update returned length */
	rep movsb			/* Copy data */
	addl	$12, %ebx
	jmp	process_ecr_params
process_ecr_params_reloc_stack:
	/* Nothing to do */
	addl	$4, %ebx
	jmp	process_ecr_params
process_ecr_params_va_list:
	/* Record current "previous list" pointer as return point for
	 * this new list.
	 */
	movl	ECR_OFFSET_PREVIOUS_VA_LIST(%esp), %eax
	movl	%eax, 4(%ebx)
	/* Record current position as "previous list" pointer */
	movl	%ebx, ECR_OFFSET_PREVIOUS_VA_LIST(%esp)
	/* Start processing new list */
	movl	0(%ebx), %ebx
	jmp	process_ecr_params
process_ecr_params_end_list:
	/* See if we should return to a previous list */
	movl	ECR_OFFSET_PREVIOUS_VA_LIST(%esp), %ebx
	testl	%ebx, %ebx
	je	process_ecr_params_done
	/* Restore "previous list" pointer from the previous list */
	movl	4(%ebx), %eax
	movl	%eax, ECR_OFFSET_PREVIOUS_VA_LIST(%esp)
	addl	$8, %ebx
	jmp	process_ecr_params
process_ecr_params_done:

	/* Grab %eax from the register store on the stack to return as
	 * function's exit code.
	 */
	movl	(ECR_OFFSET_REGISTERS+28)(%esp), %eax

ec_return_to_etherboot:	
	/* Everything that we want to keep is copied out of the stack
	 * by now, so discard it, all the way up to above the
	 * signature block.
	 */
	leal	EC_OFFSET_CALLPRESERVE(%ebp), %esp

	/* Return to C code */
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

	
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
	call	_old_real_call
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

#ifdef CONFIG_X86_64
	.arch	sledgehammer
/**************************************************************************
XSTART_lm - Transfer control to the kernel just loaded in long mode
**************************************************************************/
	.globl xstart_lm
xstart_lm:
	/* Save the callee save registers */
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	/* Switch to using physical addresses */
	call	_virt_to_phys

	/* Cache virt_offset & 0xfffff000 */
	mov	%ebp, %ebx
	andl	$0xfffff000, %ebx

	/* Initialize the page tables */
	/* Level 4 */
	leal	0x23 + pgt_level3(%ebx), %eax
	leal	pgt_level4(%ebx), %edi
	movl	%eax, (%edi)

	/* Level 3 */
	leal	0x23 + pgt_level2(%ebx), %eax
	leal	pgt_level3(%ebx), %edi
	movl	%eax, 0x00(%edi)
	addl	$4096, %eax
	movl	%eax, 0x08(%edi)
	addl	$4096, %eax
	movl	%eax, 0x10(%edi)
	addl	$4096, %eax
	movl	%eax, 0x18(%edi)

	/* Level 2 */
	movl	$0xe3, %eax
	leal	pgt_level2(%ebx), %edi
	leal	16384(%edi), %esi
pgt_level2_loop:
	movl	%eax, (%edi)
	addl	$8, %edi
	addl	$0x200000, %eax
	cmp	%esi, %edi
	jne	pgt_level2_loop

	/* Point at the x86_64 page tables */
	leal	pgt_level4(%ebx), %edi
	movl	%edi, %cr3


	/* Setup for the return from 64bit mode */
	/* 64bit align the stack */
	movl	%esp, %ebx		/* original stack pointer + 16 */
	andl	$0xfffffff8, %esp

	/* Save original stack pointer + 16 */
	pushl	%ebx

	/* Save virt_offset */
	pushl	%ebp

	
	/* Setup for the jmp to 64bit long mode */
	leal	start_lm(%ebp), %eax
	movl	%eax, 0x00 + start_lm_addr(%ebp)
	movl	$LM_CODE_SEG, %eax
	movl	%eax, 0x04 + start_lm_addr(%ebp)

	/* Setup for the jump out of 64bit long mode */
	leal	end_lm(%ebp), %eax
	movl	%eax, 0x00 + end_lm_addr(%ebp)
	movl	$FLAT_CODE_SEG, %eax
	movl	%eax, 0x04 + end_lm_addr(%ebp)

	
	/* Enable PAE mode */
	movl	%cr4, %eax
	orl	$X86_CR4_PAE, %eax
	movl	%eax, %cr4


	/* Enable long mode */
	movl	$MSR_K6_EFER, %ecx
	rdmsr
	orl	$EFER_LME, %eax
	wrmsr

	/* Start paging, entering 32bit compatiblity mode */
	movl	%cr0, %eax
	orl	$CR0_PG, %eax
	movl	%eax, %cr0

	/* Enter 64bit long mode */
	ljmp	*start_lm_addr(%ebp)
	.code64
start_lm:
	/* Load 64bit data segments */
	movl	$LM_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss

	andq	$0xffffffff, %rbx
	/* Get the address to jump to */
	movl	20(%rbx), %edx
	andq	$0xffffffff, %rdx
	
	/* Get the argument pointer */
	movl	24(%rbx), %ebx
	andq	$0xffffffff, %rbx

	/* Jump to the 64bit code */
	call	*%rdx

	/* Preserve the result */
	movl	%eax, %edx

	/* Fixup %eflags */
	cli
	cld

	/* Switch to 32bit compatibility mode */
	ljmp	*end_lm_addr(%rip)

	.code32
end_lm:
	/* Disable paging */
	movl	%cr0, %eax
	andl	$~CR0_PG, %eax
	movl	%eax, %cr0

	/* Disable long mode */
	movl	$MSR_K6_EFER, %ecx
	rdmsr
	andl	$~EFER_LME, %eax
	wrmsr

	/* Disable PAE */
	movl	%cr4, %eax
	andl	$~X86_CR4_PAE, %eax
	movl	%eax, %cr4
	
	/* Compute virt_offset */
	popl	%ebp

	/* Compute the original stack pointer + 16 */
	popl	%ebx
	movl	%ebx, %esp

	/* Enable the virtual addresses */
	leal	_phys_to_virt(%ebp), %eax
	call	*%eax

	/* Restore the callee save registers */
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp

	/* Get the C return value */
	movl	%edx, %eax

	/* Return */
	ret

	.arch i386
#endif /* CONFIG_X86_64 */

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
_OLD_REAL_CALL - Run some code in real mode.
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

	.globl _old_real_call
_old_real_call:
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
	leal	RADDR(__old_real_call)(%ebx), %edi
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
	movl	$__old_real_call, %esi
	movl	$_end16 - __old_real_call, %ecx
	leal	RADDR(__old_real_call)(%ebx), %edi
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
_old_real_call_ret:
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
__old_real_call:
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
	movw	%ax,%es
	movw	%ax,%ss
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
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	%ax, %fs
	movw	%ax, %gs

	/* Enable interrupts */
	sti

	/* Call the user supplied code */
	call	*RADDR(real_ip)

	/* Disable interrupts */
	cli

	/* Reload %ds */	
	movw	%cs, %ax
	movw	%ax, %ds

	/* Save the stack pointer */
	movw	%sp, RADDR(real_sp)
	
	/* Switch back to protected mode */
	DATA32 lgdt RADDR(real_gdtarg)
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0	/* turn on protected mode */

	/* flush prefetch queue, and reload %cs:%eip */
	DATA32 ljmp	$KERN_CODE_SEG, $_old_real_call_ret
	.code32
__end16:
	.balign 16
_end16:
	.code32

#endif /* CODE16 */

/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
	.data
	.align	4

	.globl	_gdt
	.globl	gdtarg
_gdt:
gdtarg:
	.word	_gdt_end - _gdt - 1	/* limit */
	.long	_gdt			/* addr */
	.word	0

	.globl	_pmcs
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

#ifdef CONFIG_X86_64
_lmcs:
	/* 64bit long mode code segment, base 0 */
	.word	0xffff, 0
	.byte	0x00, 0x9f, 0xaf , 0x00
_lmds:
	/* 64bit long mode data segment, base 0 */
	.word	0xffff, 0
	.byte	0x00, 0x93, 0xcf, 0x00
#endif
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

	.section ".stack"
	.p2align 3
	/* allocate a 4K stack in the stack segment */
	.globl	_stack
_stack:
	.space 4096
	.globl	_estack
_estack:
#ifdef CODE16
	/* Data that needs to be preserved across invocations of
	 * main() and potential relocation-and-restarts, such as the
	 * copy of the 16-bit prefix code and the record of base
	 * memory used by the Etherboot image.
	 */
	.section ".bss.preserve"
	.globl _prefix_copy
	.globl _eprefix_copy
	.balign 16, 0
_prefix_copy:
	.space 512
_eprefix_copy:	
	.globl image_basemem
image_basemem:
	.long 0
#endif
#ifdef CONFIG_X86_64
	.section ".bss"
	.p2align 12
	/* Include a dummy space in case we are loaded badly aligned */
	.space 4096
	/* Reserve enough space for a page table convering 4GB with 2MB pages */
pgt_level4:	
	.space 4096
pgt_level3:	
	.space 4096
pgt_level2:	
	.space 16384
start_lm_addr:
	.space	8
end_lm_addr:
	.space	8
#endif
