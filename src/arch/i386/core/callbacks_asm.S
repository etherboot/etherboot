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

	.globl	extcall_breakpoint
extcall_breakpoint:	
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
	movl	EC_SIG_TRAMPOLINE(%ebp), %ecx /* Trampoline length */
	testl	%ecx, %ecx
	jnz	2f
1:	/* No trampoline: length is &(signature) - (returned %esp) */
	movl	%ebp, %ecx	/* &(signature) */
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

#define IC_OFFSET_VA_LIST_PTR ( 0 )
#define IC_OFFSET_VA_LIST_PTR_E ( IC_OFFSET_VA_LIST_PTR + 4 )
#define IC_OFFSET_FLAGS ( IC_OFFSET_VA_LIST_PTR_E )
#define IC_OFFSET_FLAGS_E ( IC_OFFSET_FLAGS + 4 )
#define IC_OFFSET_REGISTERS ( IC_OFFSET_FLAGS_E )
#define IC_OFFSET_REGISTERS_E ( IC_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
#define IC_OFFSET_SEG_REGS ( IC_OFFSET_REGISTERS_E )
#define IC_OFFSET_SEG_REGS_E ( IC_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define IC_OFFSET_GDT ( IC_OFFSET_SEG_REGS_E )
#define IC_OFFSET_GDT_E ( IC_OFFSET_GDT + 8 )
#define IC_OFFSET_RETADDR ( IC_OFFSET_GDT_E )
#define IC_OFFSET_RETADDR_E ( IC_OFFSET_RETADDR + 8 )
#define IC_OFFSET_ORIG_STACK ( IC_OFFSET_RETADDR )
#define IC_OFFSET_OPCODE ( IC_OFFSET_ORIG_STACK + 8 )
#define IC_OFFSET_OPCODE_E ( IC_OFFSET_OPCODE + 4 )
#define IC_OFFSET_VA_LIST ( IC_OFFSET_OPCODE_E )
	
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
	/* Replace %esp in store with physical %esp value on entry */
	leal	(IC_OFFSET_ORIG_STACK - IC_OFFSET_REGISTERS)(%esp), %eax
	movl	%eax, (IC_OFFSET_REGISTERS - IC_OFFSET_REGISTERS + 12)(%esp)
	/* Store flags */
	pushfl
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
	pushfl				/* Preserve flags */
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

	/* Restore interrupt status */
	popfl

	/* Fixup the va_list pointer */
	subl	%ebp, IC_OFFSET_VA_LIST_PTR(%esp)

	/* Check opcode for EB_USE_INTERNAL_STACK flag */
	movl	IC_OFFSET_OPCODE(%esp), %eax
	testl	$EB_USE_INTERNAL_STACK, %eax
	je	1f
	/* Use internal stack flag set */
	movl	%esp, %esi		/* %esi = original stack */
	leal	_estack, %esp		/* %esp = top of internal stack */
	movl	$IC_OFFSET_OPCODE_E, %ecx /* %ecx = length to transfer */
	subl	%ecx, %esp		/* %esp = internal stack pos */
	movl	%esp, %edi		/*  = %edi */
	rep movsb			/* Copy data to internal stack */
1:

	/* Call to C code */
	call	i386_in_call

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
	/* Reset %ss and adjust %esp */
	movw	%ax, %ss
	addl	%ebp, %esp
	lret				/* Reload %cs:eip, flush prefetch */
1:

	/* Skip va_list ptr */
	popl	%eax
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

	
	
#ifdef CODE16
	.section ".text16"

	.equ	CR0_PE,1

/****************************************************************************
 * REAL-MODE CALLBACK INTERFACE
 *
 * This must be copied down to base memory in order for external
 * programs to be able to make calls in to Etherboot.  Store the
 * current physical address of Etherboot (i.e. virt_to_phys(_text)) in
 * (uint32_t)rm_etherboot_location, then copy
 * (uint16_t)rm_callback_interface_size bytes starting at
 * &((void)rm_callback_interface).
 *
 * There are two defined entry points:
 *   Offset RM_IN_CALL     = 0		Near call entry point
 *   Offset RM_IN_CALL_FAR = 2		Far call entry point
 *
 * Note that the routines _prot_to_real and _real_to_prot double as
 * trampoline fragments for external calls (calls from Etherboot to
 * real-mode code).
 ****************************************************************************
 */

	.globl	rm_callback_interface
	.code16
rm_callback_interface:
	.globl	_rm_in_call
_rm_in_call:
	jmp	_real_in_call
	.globl	_rm_in_call_far
_rm_in_call_far:
	jmp	_real_in_call_far

/****************************************************************************
 * _real_in_call
 *
 * Parameters:
 *   16-bit real-mode far return address (implicit from lcall to routine)
 *   Other parameters as for _in_call_far().
 *
 * This routine will convert the 16-bit real-mode far return address
 * to a 32-bit real-mode far return address, switch to protected mode
 * using _real_to_prot and call in to _in_call_far.
 ****************************************************************************
 */

#define RIC_PRESERVE ( 8 )
#define RIC_OFFSET_RETADDR ( RIC_PRESERVE )
#define RIC_OFFSET_RETADDR_E ( RIC_OFFSET_RETADDR + 8 )
#define RIC_ORIG_OFFSET_RETADDR_E ( RIC_OFFSET_RETADDR_E )
#define RIC_ORIG_OFFSET_RETADDR ( RIC_ORIG_OFFSET_RETADDR_E - 4 )
#define RIC_INSERT_LENGTH ( ( RIC_OFFSET_RETADDR_E - RIC_OFFSET_RETADDR ) - \
		      ( RIC_ORIG_OFFSET_RETADDR_E - RIC_ORIG_OFFSET_RETADDR ) )

/* Little bit messy that we need to know this... */
#define RIC_R2P_INSERT_LENGTH ( NUM_SEG_REGS * 2 )
	
#define RIC2_PRESERVE ( 8 )
#define RIC2_OFFSET_CALLADDR ( RIC2_PRESERVE )
#define RIC2_OFFSET_CALLADDR_E ( RIC2_OFFSET_CALLADDR + 8 )
#define RIC2_OFFSET_RETADDR ( RIC2_OFFSET_CALLADDR_E )
#define RIC2_OFFSET_RETADDR_E ( RIC2_OFFSET_RETADDR + 8 )
#define RIC2_OFFSET_OPCODE ( RIC2_OFFSET_RETADDR_E )
#define RIC2_OFFSET_OPCODE_E ( RIC2_OFFSET_OPCODE + 4 )
#define RIC2_OFFSET_PARAMS_FROM_R2P ( RIC2_OFFSET_OPCODE_E )
#define RIC2_OFFSET_ORIG_OPCODE ( RIC2_OFFSET_PARAMS_FROM_R2P + \
				  RIC_R2P_INSERT_LENGTH + 8 )
#define RIC2_INSERT_LENGTH ( RIC2_OFFSET_OPCODE_E - RIC2_OFFSET_CALLADDR )
	
	.code16
_real_in_call:
	/* Expand near return address to far return address
	 */
	subw	$2, %sp
	pushw	%bp
	pushw	%ax
	movw	%sp, %bp
	movw	%cs, %ax
	xchgw	%ax, 6(%bp)
	movw	%ax, 4(%bp)
	popw	%ax
	popw	%bp
	/* Fall through to _real_call_far */
	
	.code16
_real_in_call_far:
	/* Allow space for expanded stack */
	subw	$RIC_INSERT_LENGTH, %sp

	/* Store temporary registers */
	pushl	%ebp
	pushl	%eax

	/* 16-bit code:	need %bp as a base register */
	movw	%sp, %bp

	/* Expand 16-bit return address to 32-bit */
	movzwl	(RIC_ORIG_OFFSET_RETADDR+0)(%bp), %eax
	movl	%eax, (RIC_OFFSET_RETADDR+0)(%bp)
	movzwl	(RIC_ORIG_OFFSET_RETADDR+2)(%bp), %eax
	movl	%eax, (RIC_OFFSET_RETADDR+4)(%bp)

	/* Restore temporary registers */
	popl	%eax
	popl	%ebp

	/* Call to _real_to_prot */
	DATA32 call _real_to_prot_core

	/* Continuation point */
2:	.code32

	/* We're in protected mode with flat physical addresses and
	 * all the parameters are on the stack: set up _prot_to_real
	 * as the return address and 'ljmp' into _in_call_far.
	 */

	/* Allow space for expanded stack */
	subl	$RIC2_INSERT_LENGTH, %esp
	
	/* Store temporary registers */
	pushl	%ebp
	pushl	%eax

	/* Copy opcode, set EB_CALL_FROM_REAL_MODE.  Copy it because
	 * _in_call() and i386_in_call() expect it at a fixed
	 * position, not as part of the va_list.
	 */
	movl	RIC2_OFFSET_ORIG_OPCODE(%esp), %eax
	orl	$EB_CALL_FROM_REAL_MODE, %eax
	movl	%eax, RIC2_OFFSET_OPCODE(%esp)
	
	/* Set up call and return addresses */
	movl	$(r2p_pmcs-r2p_gdt), %eax	/* Flat physical CS */
	movl	%eax, (RIC2_OFFSET_CALLADDR+4)(%esp)
	movl	%eax, (RIC2_OFFSET_RETADDR+4)(%esp)
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp			/* %ebp = offset */
	movl	rm_etherboot_location(%ebp), %eax  /* Etherboot phys addr */
	subl	$_text, %eax
	addl	$_in_call_far, %eax		/* _in_call_far phys addr */
	movl	%eax, (RIC2_OFFSET_CALLADDR+0)(%esp)
	leal	2f(%ebp), %eax			/* return phys. address */
	movl	%eax, (RIC2_OFFSET_RETADDR+0)(%esp)
	
	/* Restore temporary registers */
	popl	%eax
	popl	%ebp

	/* Call to _in_call_far, will return via _prot_to_real */
	lret

2:	/* Return point */

	/* Remove opcode from stack */
	addl	$4, %esp
	/* Return via _prot_to_real */
	jmp	_prot_to_real

/****************************************************************************
 * rm_etherboot_location: the current physical location of Etherboot.
 * Needed so that real-mode callback routines can locate Etherboot.
 ****************************************************************************
 */
	.globl rm_etherboot_location
rm_etherboot_location:	.long 0
		
/****************************************************************************
 * _prot_to_real
 *
 * Switch from 32-bit protected mode with flat physical addresses to
 * 16-bit real mode, preserving all non-segment registers.
 *
 * This routine serves two purposes: (a) as part of the trampoline
 * code to switch down to real mode in order to make a real-mode call
 * from within Etherboot, and (b) as part of the exit path back to
 * real-mode code after a call to _real_in_call.
 * 
 * Parameters:
 *   32-bit far return address (implicit from far call to routine)
 *   seg_regs_t rm_seg_regs : structure for real-mode segment register values.
 *
 * This routine will strip rm_seg_regs from the stack (i.e. shift up
 * the return address and adjust the stack pointer).
 *
 * For case (a), the return address will eventually be interpreted by
 * _real_to_prot as part of the return to Etherboot.  It should be in
 * a flat physical segment.  (The actual segment value will be
 * ignored, but the offset must be a physical offset).  This routine
 * should be entered via a 32-bit far call.
 *
 * In case (b), the return address should be a 32-bit real mode
 * segment:offset address.  This routine should be entered via a
 * 32-bit far jump (i.e. it is given as the return point for the call
 * to _in_call_far).
 ****************************************************************************
 */

#define P2R_PRESERVE (14)
#define P2R_OFFSET_RETADDR ( P2R_PRESERVE )
#define P2R_OFFSET_RETADDR_E ( P2R_OFFSET_RETADDR + 8 )
#define P2R_OFFSET_SEG_REGS ( P2R_OFFSET_RETADDR_E )
#define P2R_OFFSET_SEG_REGS_E ( P2R_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define P2R_STRIP_LENGTH ( P2R_OFFSET_SEG_REGS_E - P2R_OFFSET_SEG_REGS )
#define P2R_NEW_OFFSET_RETADDR ( P2R_OFFSET_RETADDR + P2R_STRIP_LENGTH )

	.globl _prot_to_real
	.code32
_prot_to_real:
	/* Store interrupt status and temporary registers */
	pushfw
	pushl	%ebp
	pushl	%eax
	pushl	%ebx
	
	/* Disable interrupts */
	cli

	/* Calculate offset */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp		/* %ebp = offset */

	/* Set up GDT with real-mode limits and appropriate bases for
	 * real-mode %cs and %ss.  Reload all segment registers,
	 * adjust %esp.
	 */
	leal	p2r_gdt(%ebp), %ebx
	movl	%ebx, p2r_gdt_addr(%ebp)	/* Fixup p2r_gdtarg */
	/* Code segment */
	movzwl	(P2R_OFFSET_SEG_REGS+10)(%esp), %eax /* %eax = RM %cs */
	shll	$4, %eax		/* %eax = RM CS base */
	/* Set up protected-mode continuation point */
	leal	2f(%ebp), %ebx
	subl	%eax, %ebx
	pushl	$(p2r_rmcs - p2r_gdt)	/* Pmode CS */
	pushl	%ebx			/* Offset within new CS */
	/* Set CS base in GDT */
	movw	%ax, (p2r_rmcs+2)(%ebp)
	shrl	$16, %eax
	movb	%al, (p2r_rmcs+4)(%ebp)
	movzwl	(P2R_OFFSET_SEG_REGS+8+8)(%esp), %eax /* %eax = RM %ss */
	shll	$4, %eax		/* %eax = RM SS base */
	/* Adjust esp */
	subl	%eax, %esp		/* %esp now less than 0x10000 */
	/* Set SS base in GDT */
	movw	%ax, (p2r_rmds+2)(%ebp)
	shrl	$16, %eax
	movb	%al, (p2r_rmds+4)(%ebp)
	/* Load GDT */
	lgdt	p2r_gdt(%ebp)
	/* Reload all segment registers */
	movw	$(p2r_rmds - p2r_gdt), %ax /* Pmode DS */
	movw	%ax, %ss
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	lret				/* %cs:eip */
2:	/* Segment registers now have 16-bit limits. */
	.code16

	/* Set up bp as index register for 16-bit code */
	movw	%sp, %bp
	
	/* Set up real-mode continuation point */
	movw	(P2R_OFFSET_SEG_REGS+10)(%bp), %ax /* %ax = RM %cs */
	pushw	%ax			/* RM segment */
	call	1f
1:	popw	%bx
	addw	$2f-1b, %bx
	pushw	%bx			/* RM offset */
	movw	(P2R_OFFSET_SEG_REGS+8)(%bp), %ax /* %ax = RM %ss */

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
	addw	$P2R_OFFSET_SEG_REGS, %sp
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	subw	$(P2R_OFFSET_SEG_REGS+8), %sp

	/* Strip rm_seg_regs structure from stack, i.e. shuffle up the
	 * return address.
	 */
	movl	(P2R_OFFSET_RETADDR+0)(%bp), %eax
	movl	(P2R_OFFSET_RETADDR+4)(%bp), %ebx
	movl	%eax, (P2R_NEW_OFFSET_RETADDR+0)(%bp)
	movl	%ebx, (P2R_NEW_OFFSET_RETADDR+4)(%bp)
	
	/* Restore registers and interrupt status */
	popl	%ebx
	popl	%eax
	popl	%ebp
	popfw

	/* Adjust stack to point to return address */
	addw	$P2R_STRIP_LENGTH, %sp

	jmp	99f
p2r_gdt:
p2r_gdtarg:
p2r_gdt_limit:		.word p2r_gdt_end - p2r_gdt - 1
p2r_gdt_addr:		.long 0
p2r_gdt_padding:	.word 0
p2r_rmcs:
	/* 16 bit real mode code segment */
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x9b,0x00,(0>>24)
p2r_rmds:
	/* 16 bit real mode data segment */
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x93,0x00,(0>>24)
p2r_gdt_end:
99:	
	
	/* This is the end of the trampoline prefix code.  When used
	 * as a prefix, fall through to the following code in the
	 * trampoline.
	 */
	.globl	_prot_to_real_end
_prot_to_real_end:

	/* For the case that this is being used as the return path to
	 * real-mode code, we now have a 32-bit real-mode far return
	 * address on the stack.
	 */
	DATA32 lret			/* Back to real-mode caller */
	
/****************************************************************************
 * _real_to_prot
 *
 * Switch from 16-bit real-mode to 32-bit protected mode with flat
 * physical addresses, preserving all registers, and return to
 * Etherboot (or other specified address).
 *
 * This routine serves two purposes: (a) as part of the trampoline
 * code to switch back from real mode after making a real-mode call
 * from within Etherboot, and (b) as part of the entry path from
 * real-mode code to in_call().
 * 
 * Parameters:
 *   32-bit flat physical far return address
 *
 * This routine will push a seg_regs_t structure containing the
 * real-mode segment registers onto the stack before returning to the
 * specified address.
 ****************************************************************************
 */

#define R2P_PRESERVE ( 12 )
#define R2P_OFFSET_LOCALRETADDR ( R2P_PRESERVE )
#define R2P_OFFSET_LOCALRETADDR_E ( R2P_OFFSET_LOCALRETADDR + 4 )
#define R2P_OFFSET_RETADDR ( R2P_OFFSET_LOCALRETADDR_E )
#define R2P_OFFSET_RETADDR_E ( R2P_OFFSET_RETADDR + 8 )
#define R2P_OFFSET_SEG_REGS ( R2P_OFFSET_RETADDR_E )
#define R2P_OFFSET_SEG_REGS_E ( R2P_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define R2P_INSERT_LENGTH ( R2P_OFFSET_SEG_REGS_E - R2P_OFFSET_SEG_REGS )
#define R2P_ORIG_OFFSET_LOCALRETADDR ( R2P_OFFSET_LOCALRETADDR + \
				       R2P_INSERT_LENGTH )
#define R2P_ORIG_OFFSET_RETADDR ( R2P_OFFSET_RETADDR + R2P_INSERT_LENGTH )

/* Safety check: _real_in_call needs to know R2P_INSERT_LENGTH but it
 * doesn't get calculated until this point in the file.  Rather than
 * reordering everything, just give a fatal error if it was the wrong
 * value.
 */
#if RIC_R2P_INSERT_LENGTH != R2P_INSERT_LENGTH
#error RIC_R2P_INSERT_LENGTH != R2P_INSERT_LENGTH
#endif

	.globl _real_to_prot
	.globl _ereal_to_prot
	
	.code16		
_real_to_prot:
	DATA32 call _real_to_prot_core
	.code32
	/* Force return segment address to 0x08 (i.e. physical).  We
	 * do this because the GDT may have changed as a result of the
	 * real-call payload.  A physical offset address for the
	 * return address is an entry requirement of ours anyway, and
	 * ext_call() can cope with an altered GDT on return.
	 */
	movl	$(r2p_pmcs-r2p_gdt), 4(%esp)
	
	/* Return to Etherboot */
	lret

	.code16
_real_to_prot_core:	
	/* Allow space for real-mode segment registers */
	subw	$R2P_INSERT_LENGTH, %sp
	
	/* Store interrupt status and temporary registers */
	pushfw
	pushw	%bp
	pushl	%eax
	pushl	%ebx

	/* Move return addresses down the stack so we can insert the
	 * segment register dump.
	 */
	movw	%sp, %bp
	movl	(R2P_ORIG_OFFSET_RETADDR+0)(%bp), %eax
	movl	(R2P_ORIG_OFFSET_RETADDR+4)(%bp), %ebx
	movl	%eax, (R2P_OFFSET_RETADDR+0)(%bp)
	movl	%ebx, (R2P_OFFSET_RETADDR+4)(%bp)
	movl	(R2P_ORIG_OFFSET_LOCALRETADDR)(%bp), %eax
	movl	%eax, (R2P_OFFSET_LOCALRETADDR)(%bp)

	/* Store segment registers on stack */
	addw	$R2P_OFFSET_SEG_REGS_E, %sp
	pushw	%cs
	pushw	%ss
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs
	subw	$R2P_OFFSET_SEG_REGS, %sp

	/* Calculate 32-bit continuation address and push onto stack */
	xorl	%eax, %eax
	movw	%cs, %ax
	shll	$4, %eax		/* %eax = %cs << 4 */
	addl	%eax, R2P_OFFSET_LOCALRETADDR(%bp) /* Adjust local ret. addr */
	pushl	$(r2p_pmcs-r2p_gdt)	/* PMCS from r2p_gdt */
	call	1f
1:	popw	%bp			/* %bp = offset */
	leal	(2f-1b)(%bp), %ebx
	addl	%eax, %ebx
	pushl	%ebx			/* Continuation offset address */

	/* Convert %esp to 32-bit physical value */
	xorl	%eax, %eax
	movw	%ss, %ax
	shll	$4, %eax
	movzwl	%sp, %esp
	addl	%eax, %esp

	/* Set up GDT */
	leal	(r2p_gdt-1b)(%bp), %eax	/* %cs:eax = %cs:ax = &(r2p_gdt) */
	movl	%cs, %ebx
	shll	$4, %ebx
	addl	%ebx, %eax		/* %eax = &r2p_gdt (physical) */
	movl	%eax, %cs:(2+r2p_gdt-1b)(%bp) /* Set phys. addr. in r2p_gdt */

	/* Load GDT */
	lgdt	%cs:(r2p_gdt-1b)(%bp)	/* Load GDT */

	/* Switch to protected mode */
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0

	/* Reload %ss and %cs:eip */
	movw	$(r2p_pmds-r2p_gdt), %ax /* PMDS from r2p_gdt */
	movw	%ax, %ss		/* Reload %ss */
	DATA32 lret			/* 'ljmp' to 2f */
2:
	.code32
	/* Reload other segment registers with protected-mode values */
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	/* Restore registers and interrupt status */
	popl	%ebx
	popl	%eax
	popw	%bp
	popfw

	/* Return from _real_to_prot_core */
	ret
r2p_gdt:
	.word	r2p_gdt_end - r2p_gdt - 1	/* limit */
	.long 0					/* addr */
	.word 0
r2p_pmcs:
	/* 32 bit protected mode code segment, physical addresses */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0
r2p_pmds:
	/* 32 bit protected mode data segment, physical addresses */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0
r2p_gdt_end:

	.globl	_real_to_prot_end
_real_to_prot_end:	

rm_callback_interface_end:

	.globl	rm_callback_interface_size
rm_callback_interface_size:
	.word	rm_callback_interface_end - rm_callback_interface

/* Useful for calculating breakpoints */
	.equ	p2r_size, _prot_to_real_end - _prot_to_real
	.equ	r2p_size, _real_to_prot_end - _real_to_prot
	.equ	trampoline_base_size, EC_SIGBLOCK_LENGTH + p2r_size + r2p_size

/****************************************************************************
 * END OF REAL-MODE CALLBACK INTERFACE
 ****************************************************************************
 */


#ifdef PXE_EXPORT
/****************************************************************************
 * PXE CALLBACK INTERFACE
 *
 * Prepend this to rm_callback_interface to create a real-mode PXE
 * callback interface.
 ****************************************************************************
 */
	.globl	pxe_callback_interface
	.code16
pxe_callback_interface:	

	/* in_call mechanism for !PXE API calls */
	.globl	_pxe_in_call_far
_pxe_in_call_far:
	/* Prepend "PXE API call" and "API version 0x201" to stack */
	pushl	$0x201
	jmp	1f
	/* in_call mechanism for PXENV+ API calls */
	.globl	_pxenv_in_call_far
_pxenv_in_call_far:
	/* Prepend "PXE API call" and "API version 0x200" to stack */
	pushl	$0x200
1:	pushl	$EB_OPCODE_PXE
	/* Perform real-mode in_call */
	call	pxe_rm_in_call
	/* Return */
	addw	$8, %sp
	lret

pxe_rm_in_call:	
pxe_attach_rm:
	/* rm_callback_interface must be appended here */

pxe_callback_interface_end:

	.globl	pxe_callback_interface_size
pxe_callback_interface_size:
	.word	pxe_callback_interface_end - pxe_callback_interface
	
#endif /* PXE_EXPORT */

	
/* Temporary demo payload routine */
	
	.globl hello_world
	.globl ehello_world
	.code16
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

	.globl	trial_real_incall
	.globl	etrial_real_incall
	.code16
trial_real_incall:
	pushw	$0x0000
	pushw	$0x7c00
	pushw	$0x3456
	pushl	$0x201
	pushl	$EB_OPCODE_PXE
	lcall	$0x8000, $RM_IN_CALL_FAR
	addw	$14, %sp
etrial_real_incall:	

	
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
