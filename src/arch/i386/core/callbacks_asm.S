/* Callout/callback interface for Etherboot
 *
 * This file provides the mechanisms for making calls from Etherboot
 * to external programs and vice-versa.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "callbacks.h"

/* FIXME: remove these once we have relocated _gdt */
#define KERN_CODE_SEG (0x08)
#define KERN_DATA_SEG (0x10)

#undef CODE16
#if defined(PCBIOS) || defined(TAGGED_IMAGE)
#define	CODE16
#endif

#define NUM_PUSHA_REGS (8)
#define NUM_SEG_REGS (6)

	.text
	.arch i386
	.code32

/****************************************************************************
 * _v_ext_call
 *
 * Call to an external routine.  Has the capability to pass in and out
 * register and stack contents, relocate stack to a new address,
 * switch and restore GDT etc.
 ****************************************************************************
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
#define EC_OFFSET_CALLPRESERVE ( EC_OFFSET_SIGBLOCK + EC_SIGBLOCK_LENGTH )
#define EC_CALLPRESERVE_LENGTH (16)
#define EC_OFFSET_RETADDR ( EC_OFFSET_CALLPRESERVE + EC_CALLPRESERVE_LENGTH )
#define EC_RETADDR_LENGTH (4)
#define EC_OFFSET_ARGUMENTS ( EC_OFFSET_RETADDR + EC_RETADDR_LENGTH )

/* Offsets from current %esp */
/* Outgoing structure */
#define EC_OFFSET_FLAGS 0
#define EC_OFFSET_REGISTERS ( EC_OFFSET_FLAGS + 4 )
#define EC_OFFSET_SEG_REGS ( EC_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
#define EC_OFFSET_ADDRESSES ( EC_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define EC_OFFSET_PARAMS ( EC_OFFSET_ADDRESSES + 16 )
/* Return structure */
#define ECR_OFFSET_FLAGS 0
#define ECR_OFFSET_REGISTERS ( ECR_OFFSET_FLAGS + 4 )
#define ECR_OFFSET_SEG_REGS ( ECR_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
#define ECR_OFFSET_PARAMS ( ECR_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )

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
	 * ext_params		%esp + EC_OFFSET_PARAMS
	 * addresses		%esp + EC_OFFSET_ADDRESSES
	 * seg_registers	%esp + EC_OFFSET_SEG_REGS
	 * registers		%esp + EC_OFFSET_REGISTERS
	 * flags		%esp + EC_OFFSET_FLAGS ( = %esp + 0 )
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
	 * ext_params : Parameters to pass on stack to external call.
	 * "length" indicates the length of this data, which may not
	 * be a multiple of 4.  The bottom of ext_params is kept
	 * aligned, i.e. there is padding at the top.
	 *
	 * addresses : return and call addresses.  Always 4 dwords.
	 * For a near call, these are (return address, call address,
	 * 0, call address); for a far call (return segment, return
	 * address, call segment, call address).
	 *
	 * seg_registers : segment registers to set up before calling
	 * to external routine.  %cs and %ss are dealt with
	 * separately.  6 words (i.e. 3 dwords).
	 *
	 * registers : registers to set up before calling to external
	 * routine.  8 dwords, suitable for a single popal
	 * instruction.
	 *
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
	pushl	%eax		/* Signature */
	pushl	%ecx		/* trampoline */
	pushl	%ebx		/* stack_ptr */
	pushl	%ecx		/* length */
	pushl	%eax		/* Signature */
	movl	%esp, %ebp	/* and preserve stack position in %ebp */

	/* Save default addresses for a near call */
	leal	ec_return, %eax
	pushl	%eax		/* Near return address */
	pushl	(EC_OFFSET_SIGBLOCK+EC_OFFSET_ARGUMENTS)(%ebp)
				/* Near call address */
	xorl	%eax, %eax
	pushl	%eax		/* 0 (indicating near call) */
	pushl	(EC_OFFSET_SIGBLOCK+EC_OFFSET_ARGUMENTS)(%ebp)
				/* Near call address */

	/* Save segment registers.  Order matches the seg_regs_t
	 * struct in callbacks.h
	 */
	pushw	%ax		/* %cs = 0 => "near call" */
	pushw	%ss
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs

	/* Save registers.  (Current register contents are fairly
	 * meaningless, but using pushal is the most efficient way to
	 * allocate the stack space.)
	 */
	pushal

	/* Save flags */
	pushfl

	/* Set %ebx to point to first argument in list */
	movl	(EC_OFFSET_ARGUMENTS+4)(%ebp), %ebx
	
	/* Mask interrupts around non-reentrant code */
	cli

	/* Ensure that movsb etc work as expected */
	cld

	/* Loop to process parameters to _ext_call */
process_ec_params:
	movl	0(%ebx), %eax		/* %eax = type */
	addl	$4, %ebx
	cmpw	$EXTCALL_END_LIST, %ax
	je	process_ec_params_done
	cmpl	$EXTCALL_NONE, %eax
	je	process_ec_params
	cmpl	$EXTCALL_REGISTERS, %eax
	je	process_ec_params_registers
	cmpl	$EXTCALL_SEG_REGISTERS, %eax
	je	process_ec_params_seg_registers
	cmpl	$EXTCALL_GDT, %eax
	je	process_ec_params_gdt
	cmpl	$EXTCALL_STACK, %eax
	je	process_ec_params_stack
	cmpl	$EXTCALL_RET_STACK, %eax
	je	process_ec_params_ret_stack
	cmpl	$EXTCALL_RELOC_STACK, %eax
	je	process_ec_params_reloc_stack
	cmpl	$EXTCALL_TRAMPOLINE, %eax
	je	process_ec_params_trampoline
	/* fall through to error */
process_ec_params_error: 
	/* Lockup machine.  Drastic, but since we may have reloaded
	 * the GDT already, there's not a lot else we can safely do.
	 * If the parameters pass the checks in _ext_call_check() then
	 * this should never happen anyway.  We re-enable interrupts
	 * so that Ctrl-Alt-Del works.
	 */
	sti
1:	jmp	1b
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
process_ec_params_seg_registers:
	movl	0(%ebx), %esi		/* %esi = &seg_registers */
	addl	$4, %ebx
	movl	$NUM_SEG_REGS, %ecx
	leal	EC_OFFSET_SEG_REGS(%esp), %edi
	rep movsw
	/* Recalculate return address based on new CS (fall-through) */
ec_recalc_addresses:
	/* Check code segment register, set near/far call addresses */
	call	1f
1:	popl	%eax
	subl	$1b, %eax		/* %eax = current offset */
	leal	ec_return(%eax), %ecx	/* %ecx = return address */
	movl	(EC_OFFSET_ARGUMENTS)(%ebp), %edx /* %edx = call address */
	movzwl	(EC_OFFSET_SEG_REGS+10)(%esp), %eax /* %eax = '%cs' */
	testl	%eax, %eax
	jne	1f
	/* near call */
	movl	%edx, (EC_OFFSET_ADDRESSES+0)(%esp)	/* call address */
	movl	%eax, (EC_OFFSET_ADDRESSES+4)(%esp)	/* zero */
	movl	%edx, (EC_OFFSET_ADDRESSES+8)(%esp)	/* call address */
	movl	%ecx, (EC_OFFSET_ADDRESSES+12)(%esp)	/* return address */
	jmp	process_ec_params
1:	/* far call */
	movl	%edx, (EC_OFFSET_ADDRESSES+0)(%esp)	/* call address */
	movl	%eax, (EC_OFFSET_ADDRESSES+4)(%esp)	/* call segment */
	pushl	%cs
	popl	%eax
	movl	%ecx, (EC_OFFSET_ADDRESSES+8)(%esp)	/* return address */
	movl	%eax, (EC_OFFSET_ADDRESSES+12)(%esp)	/* return segment */
	jmp	process_ec_params
process_ec_params_gdt:
	movl	0(%ebx), %eax		/* %eax = &gdt */
	lgdt	(%eax)			/* Load new GDT */
	/* Calculate eip offset for new CS and ljmp to reload */
	movl	4(%ebx), %ecx		/* %ecx = replacement %cs */
	addl	%ecx, %eax		/* %eax = &(code segment descriptor) */
	movb	7(%eax), %dh
	movb	4(%eax), %dl
	shll	$16, %edx
	movw	2(%eax), %dx		/* %edx = new %cs base address */
	subl	virt_offset, %edx	/* %edx = offset to new base address */
	leal	2f, %eax		/* %eax = new %eip for 2f */
	subl	%edx, %eax
	pushl	%ecx			/* new %cs */
	pushl	%eax			/* new %eip */
	lret				/* and ljmp */
2:	/* Don't reset the segment registers yet; they will be
	 * reloaded just before the external call.  For now, we still
	 * want to be able to access the Etherboot data and stack
	 * segments.  Interrupts are disabled so we're safe doing
	 * this.
	 */
	addl	$8, %ebx
	/* Recalculate return address based on new CS */
	jmp	ec_recalc_addresses
process_ec_params_stack:
	/* Move data currently on stack down to accommodate new data */
	movl	EC_SIG_LENGTH(%ebp), %ecx /* %ecx = current data length */
	movl	4(%ebx), %edx		/* %edx = extra data length */
	addl	$EC_OFFSET_PARAMS, %ecx	/* %ecx = total data len to copy */
	movl	%esp, %esi		/* Source location */
	movl	%ebp, %edi
	subl	%ecx, %edi
	subl	%edx, %edi
	andl	$~0x3, %edi		/* Destination location */
	movl	%edi, %esp		/* = new stack location */
	rep movsb			/* Shift stack down to make room */
	movl	%edx, %ecx		/* Extra data length */
	movl	0(%ebx), %esi		/* Extra data source */
	rep movsb			/* Copy in new data */
	addl	%edx, EC_SIG_LENGTH(%ebp) /* Update length */
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
process_ec_params_trampoline:
	/* Record current value of length as trampoline offset.
	 * Ignore if a trampoline is already defined.  (This is to
	 * make it possible to concatenate trampoline sections
	 * together.)
	 */
	movl	EC_SIG_TRAMPOLINE(%ebp), %eax
	testl	%eax, %eax
	jne 1f
	movl	EC_SIG_LENGTH(%ebp), %eax
	movl	%eax, EC_SIG_TRAMPOLINE(%ebp)
	/* Process as a EXTCALL_STACK parameter */
1:	jmp	process_ec_params_stack
process_ec_params_done:
	/* Stack is all set up as described above.  Copy to new
	 * location, set up the register values and jump to the
	 * function.
	 */

	/* Copy stack to new location indicated by stack_ptr, reset
	 * %ss:esp to point to it.
	 */
	movw	(EC_OFFSET_SEG_REGS+8)(%esp), %ax
	movw	%ax, %es		/* %es = new %ss */
	movl	%esp, %esi		/* %esi = &(original stack) */
	leal	EC_SIGBLOCK_LENGTH(%ebp), %edi /* %edi = &(orig. stack top) */
	movl	%edi, %ecx
	subl	%esp, %ecx		/* %ecx = length */
	xchgl	EC_SIG_STACK(%ebp), %edi /* %es:edi = &(new stack top),	   */
					 /* stack_ptr = &(orig. stack top) */
	subl	%ecx, %edi		/* %es:edi = &(new stack) */
	movl	%edi, %esp		/* Set new %esp */
	rep ss movsb			/* Copy stack */
	movw	%ax, %ss		/* Set new %ss */
	leal	(-EC_SIGBLOCK_LENGTH)(%edi), %ebp /* Set new %ebp */

	/* Check for null call address => use trampoline, edit call
	 * address as appropriate.
	 */
	cmpl	$0, (EC_OFFSET_ADDRESSES+0)(%esp)
	jne	1f			/* No trampoline */
	/* Zero call address indicates use of trampoline code */
	movl	%ss:EC_SIG_TRAMPOLINE(%ebp), %eax
					/* %eax = (trampoline-params) */
	addl	$EC_OFFSET_PARAMS, %eax	/* %eax = (trampoline-%esp) */
	addl	%esp, %eax		/* %eax = trampoline address */
	/* Note: we are requiring that cs and ss have same base
	 * address when using trampoline code
	 */
	movl	%eax, (EC_OFFSET_ADDRESSES+0)(%esp)
	cmpl	$0, (EC_OFFSET_ADDRESSES+4)(%esp)
	jne	1f
	/* Near call only: update near call address field */
	movl	%eax, (EC_OFFSET_ADDRESSES+8)(%esp)
1:

	/* Restore flags.  (It is safe to do this; cs and ss are
	 * already reloaded, and we don't use the other segment
	 * registers before they get reloaded anyway.)
	 */
	popfl
	
	/* Restore general-purpose registers */
	popal

	/* Restore segment registers.  This will also trigger a reload
	 * from the new GDT, if there is one.
	 */
	popw	%gs			/* Do not access these segment	*/
	popw	%fs			/* registers after this point.	*/
	popw	%es			/* This means no references to	*/
	popw	%ds			/* non-stack variables.		*/
	addl	$4, %esp		/* Skip ss and cs */

	/* Call to routine */
	cmpl	$0, 4(%esp)
	jne	1f
	/* near call */

	addl	$8, %esp		/* Skip the zeroes */
	ret				/* Do the calculated call */
1:	/* far call */
	lret				/* Do the calculated lcall */

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

	/* Record segment registers */
	pushw	%cs		/* Meaningless, include for padding */
	pushw	%ss
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs

	/* Record general-purpose registers */
	pushal

	/* Record flags */
	pushfl
	
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
	movl	%ebp, %ecx	/* &(signature) */
	leal	ECR_OFFSET_PARAMS(%esp), %edx /* Returned %esp */
	subl	%edx, %ecx
	movl	%ecx, EC_SIG_LENGTH(%ebp) /* Store length */

	/* Align stack by shifting down */
	addl	$ECR_OFFSET_PARAMS, %ecx /* Length to copy */
	movl	%esp, %esi
	andl	$~0x3, %esp
	movl	%esp, %edi
	rep movsb		/* Shift stack down */
	/* Stack aligned, length set as before, structures in place */

	/* Reload GDT, copy back stack and reset segment registers. */
	pushfw
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
	movl	EC_SIG_STACK(%ebp), %edi
	movl	%edi, %ebp
	subl	%ecx, %edi	/* %es:edi = dest address */
	subl	$EC_SIGBLOCK_LENGTH, %ebp /* New %ebp */
	movl	%edi, %esp	/* New %esp */
	rep ss movsb
	movw	%ax, %ss	/* Reload %ss */
	popfw			/* Restore interrupt status */

	/* %ebx now points to first parameter in list, as before */
	movl	(EC_OFFSET_ARGUMENTS+4)(%ebp), %ebx

	/* Reprocess argument list */
process_ecr_params:
	movl	0(%ebx), %eax		/* %eax = type */
	addl	$4, %ebx
	cmpl	$EXTCALL_END_LIST, %eax
	je	process_ecr_params_done
	cmpl	$EXTCALL_NONE, %eax
	je	process_ecr_params
	cmpl	$EXTCALL_REGISTERS, %eax
	je	process_ecr_params_registers
	cmpl	$EXTCALL_SEG_REGISTERS, %eax
	je	process_ecr_params_seg_registers
	cmpl	$EXTCALL_GDT, %eax
	je	process_ecr_params_gdt
	cmpl	$EXTCALL_STACK, %eax
	je	process_ecr_params_stack
	cmpl	$EXTCALL_RET_STACK, %eax
	je	process_ecr_params_ret_stack
	cmpl	$EXTCALL_RELOC_STACK, %eax
	je	process_ecr_params_reloc_stack
	cmpl	$EXTCALL_TRAMPOLINE, %eax
	je	process_ecr_params_trampoline
	/* fall through to error */
process_ecr_params_error:
	/* Lockup machine.  Drastic, but since we may have the wrong
	 * GDT, there's not a lot else we can safely do.  This should
	 * only happen if the external routine has corrupted our
	 * stack, in which case we're going to crash at some point
	 * anyway; better now when it can be traced rater than at a
	 * random future point.
	 */
	sti
1:	jmp	1b
process_ecr_params_registers:
	/* Copy registers from register store on stack to C structure */
	movl	0(%ebx), %edi		/* %edi = &registers */
	addl	$4, %ebx
	movl	$NUM_PUSHA_REGS, %ecx
	leal	ECR_OFFSET_REGISTERS(%esp), %esi
	rep movsl
	jmp	process_ecr_params
process_ecr_params_seg_registers:
	/* Copy segment registers from segment register store on stack
	 * to C structure
	 */
	movl	0(%ebx), %edi		/* %edi = &seg_registers */
	addl	$4, %ebx
	movl	$NUM_SEG_REGS, %ecx
	leal	ECR_OFFSET_SEG_REGS(%esp), %esi
	rep movsw
	jmp	process_ecr_params
process_ecr_params_gdt:
	/* Nothing to do */
	addl	$8, %ebx
	jmp	process_ecr_params
process_ecr_params_stack:
	/* Nothing to do */
	addl	$8, %ebx
	jmp	process_ecr_params
process_ecr_params_ret_stack: 
	/* Copy returned stack to C structure.  Be careful of maximum
	 * structure length
	 */
	movl	EC_SIG_LENGTH(%ebp), %ecx /* %ecx = returned stack length */
	leal	ECR_OFFSET_PARAMS(%esp), %esi /* %esi = &(returned stack) */
	movl	0(%ebx), %edi		/* %edi = &(C structure) */
	movl	8(%ebx), %eax		/* %eax = &(C length variable) */
	testl	%eax, %eax
	je	1f
	movl	%ecx, (%eax)		/* Store length if applicable */
1:	movl	4(%ebx), %eax		/* %eax = max length allowed */
	cmpl	%eax, %ecx		/* Check length against max length */
	jbe	2f
	movl	%eax, %ecx		/* ...and truncate if necessary */
2:	rep movsb			/* Copy data */
	addl	$12, %ebx
	jmp	process_ecr_params
process_ecr_params_reloc_stack:
	/* Nothing to do */
	addl	$4, %ebx
	jmp	process_ecr_params
process_ecr_params_trampoline:
	/* Nothing to do */
	addl	$8, %ebx
	jmp	process_ecr_params
process_ecr_params_done:

	/* Grab %eax from the register store on the stack to return as
	 * function's exit code.
	 */
	movl	(ECR_OFFSET_REGISTERS+28)(%esp), %eax

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

/****************************************************************************
 * _in_call
 *
 * Make a call in to Etherboot.  There are two 32-bit entry points:
 * _in_call and _in_call_far, for near calls and far calls
 * respectively.  Both should be called with flat physical addresses.
 * They will result in a call to the C routine in_call(); see there
 * for API details.
 ****************************************************************************
 */

#define IC_OFFSET_FLAGS ( 0 )
#define IC_OFFSET_REGISTERS ( IC_OFFSET_FLAGS + 4 )
#define IC_OFFSET_SEG_REGS ( IC_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
#define IC_OFFSET_GDT ( IC_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define IC_OFFSET_RETADDR ( IC_OFFSET_GDT + 8 )
	
	.globl _in_call
	.globl _in_call_far
_in_call:
	pushl	$0			/* Indicate near return address */
_in_call_far:
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
	/* Store flags */
	pushfl

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
	pushfl				/* Preserve flags */
	cli				/* Temporarily disable interrupts */
	lgdt	%cs:gdtarg(%ebp)

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

	/* Restore interrupt status */
	popfl

	/* Call to C code */
	call	in_call

	/* Set %eax (return code from C) in registers structure on
	 * stack, so that we return it to the caller.
	 */
	movl	%eax, (IC_OFFSET_REGISTERS + 28)(%esp)

	/* Calculate physical continuation address */
	movl	virt_offset, %ebp
	movzwl	(IC_OFFSET_SEG_REGS + 10)(%esp), %eax	/* %cs */
	pushl	%eax			/* Continuation segment */
	leal	1f(%ebp), %eax
	pushl	%eax			/* Continuation offset */
	movw	(IC_OFFSET_SEG_REGS + 8)(%esp), %ax	/* %ss */
	
	/* Restore caller's GDT */
	cli				/* Temporarily disable interrupts */
	lgdt	(8+IC_OFFSET_GDT)(%esp)
	movw	%ax, %ss
	addl	%ebp, %esp
	lret				/* Reload %cs:eip, flush prefetch */
	
1:	/* Reset %ss and adjust %esp */

	/* Restore flags (including revert of interrupt status) */
	popfl
	/* Reload general-purpose registers to be returned */
	popal
	/* Reload segment registers as passed in from caller */
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	addl	$(4+8), %esp	/* Skip %cs, %ss and GDT (already reloaded) */
	
	cmpl	$0, 0(%esp)		/* Check for near/far return */
	je	2f
1:	/* far return */
	lret
2:	/* near return */
	addl	$4, %esp
	ret

	
	
#ifdef CODE16

	.equ	CR0_PE,1
		
/****************************************************************************
 * _prot_to_real
 *
 * (trampoline prefix)
 *
 * Switch from 16-bit protected mode to 16-bit real mode, preserving
 * all registers.
 *
 * This fragment should be called in a 16-bit code segment,
 * paragraph-aligned.  All other segments should be 16-bit data, with
 * the same base address.  Segment limits should be real-mode
 * compatible, i.e. 0xffff.  This implies that %eip and %esp will
 * already have the top 16 bits cleared when called.
 * 
 * Assuming that you intend to return via _real_to_prot, the return
 * address should be a physical address and the call type should be
 * far (although the segment of the return address will be ignored).
 *
 * Parameter:
 *	RMCS (word) : (segment base address >> 4), i.e. real-mode %cs value
 ****************************************************************************
 */
	.section ".text16"
	.code16
	.globl _prot_to_real
	.globl _eprot_to_real
_prot_to_real:
	/* Store interrupt status and temporary registers */
	pushfw
	pushw	%bp
	pushl	%eax
	pushl	%ebx
	movw	%sp, %bp
	
	/* Disable interrupts */
	cli

	/* Set up real-mode continuation point */
	movw	20(%bp), %ax
	pushw	%ax			/* RM segment */
	call	1f
1:	popw	%bx
	addw	$2f-1b, %bx
	pushw	%bx			/* RM offset */

	/* Switch to real mode */
	movl	%cr0, %ebx
	andb	$0!CR0_PE, %bl
	movl	%ebx, %cr0
	
	/* Load real-mode segment value to %ss.  %sp already OK */
	xorl	%ebx, %ebx		/* delay after rmode switch */
	movw	%ax, %ss

	/* 'ljmp' to reload %cs:ip and flush prefetch queue */	
	lret
2:	

	/* Reload other segment registers with real-mode values */
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	/* Shuffle return address up the stack (i.e. remove our
	 * parameter, the real-mode segment value, from the parameter
	 * list.)
	 */
	movl	16(%bp), %eax
	movl	12(%bp), %ebx
	movl	%eax, 18(%bp)
	movl	%ebx, 14(%bp)
	
	/* Restore registers and interrupt status */
	popl	%ebx
	popl	%eax
	popw	%bp
	popfw

	/* Adjust stack to point to return address */
	addw	$2, %sp

	/* fall through to code following trampoline prefix */
_eprot_to_real:
	.code32
	.previous

/****************************************************************************
 * _real_to_prot
 *
 * (trampoline suffix)
 *
 * Switch from 16-bit real-mode to 32-bit protected mode with flat
 * physical addresses, preserving all registers, and return to
 * Etherboot.
 */
	.section ".text16"
	.code16
	.globl _real_to_prot
	.globl _ereal_to_prot
_real_to_prot:
	/* Store interrupt status and temporary registers */
	pushfw
	pushw	%bp
	pushl	%eax
	pushl	%ebx

	call	1f
1:	pop	%bp			/* %bp = offset */

	/* Calculate 32-bit continuation address and push onto stack */
	pushl	$0x08			/* PMCS from rtp_gdt */
	xorl	%eax, %eax
	movw	%cs, %ax
	shll	$4, %eax
	leal	(2f-1b)(%bp), %ebx
	addl	%eax, %ebx
	pushl	%ebx

	/* Convert %esp to 32-bit physical value */
	xorl	%eax, %eax
	movw	%ss, %ax
	shll	$4, %eax
	movzwl	%sp, %esp
	addl	%eax, %esp

	/* Set up GDT */
	leal	(rtp_gdt-1b)(%bp), %eax	/* %cs:eax = %cs:ax = &(rtp_gdt) */
	movl	%cs, %ebx
	shll	$4, %ebx
	addl	%ebx, %eax		/* %eax = &rtp_gdt (physical) */
	movl	%eax, %cs:(2+rtp_gdt-1b)(%bp) /* Set phys. addr. in rtp_gdt */

	/* Load GDT */
	lgdt	%cs:(rtp_gdt-1b)(%bp)	/* Load GDT */

	/* Switch to protected mode */
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0

	/* Reload %ss and %cs:eip */
	movw	$0x10, %ax		/* PMDS from rtp_gdt */
	movw	%ax, %ss		/* Reload %ss */
	DATA32 lret			/* 'ljmp' to 2f */
2:
	.code32
	/* Reload other segment registers with protected-mode values */
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	/* Force return segment address to 0x08 (i.e. physical).  We
	 * do this because the GDT may have changed as a result of the
	 * real-call payload.  A physical offset address for the
	 * return address is an entry requirement of ours anyway, and
	 * ext_call() can cope with an altered GDT on return.
	 */
	movl	$0x08, 16(%esp)
	
	/* Restore registers and interrupt status */
	popl	%ebx
	popl	%eax
	popw	%bp
	popfw

	/* Return to Etherboot */
	lret
rtp_gdt:
	.word	rtp_gdt_end - rtp_gdt - 1	/* limit */
	.long 0					/* addr */
	.word 0
rtp_pmcs:
	/* 32 bit protected mode code segment, physical addresses */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0
rtp_pmds:
	/* 32 bit protected mode data segment, physical addresses */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0
rtp_gdt_end:

_ereal_to_prot:	
	.code32
	.previous



/* Temporary demo payload routine */
	
	.section ".text16"
	.code16
	.globl hello_world
	.globl ehello_world
hello_world:
	call	1f
1:	popw	%bp
	movw	$0x0007, %bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah		/* write char, tty mode */
	leaw	(10f-1b)(%bp), %si
	movw	$(20f-10f), %cx
2:	lodsb
	int	$0x10
	loop	2b
	jmp	99f
10:	.ascii "hello world!\r\n"
20:	
99:	
	movw	$0xaaaa, %ax
ehello_world:
	.code32
	.previous

	
#endif /* CODE16 */
	
	
	.globl demo_extcall
demo_extcall:
	
	movl	%ebx, %ecx
/*	lret */
	
	popl	%ebx			/* return address */
	popl	%edx

	mov	$0x10, %ax
	mov	%ax, %ds
	mov	%ax, %es
	movl	0(%esp), %eax
/*	addl	4(%esp), %eax
	addl	8(%esp), %eax
	addl	11(%esp), %eax */
	pushw	$0xabcd 
	pushw	$0x5678
	pushw	$0x1234
	pushw	%ss
	pushw	%cs
	pushl	%edx
	pushl	%ebx
	lret
/*	.byte 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99*/
	.globl demo_extcall_end
demo_extcall_end:		

	.globl get_esp
get_esp:
	movl	%esp, %eax
	ret
