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
#define FLAT_CODE_SEG (0x28)
#define FLAT_DATA_SEG (0x30)
	
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
 * Call to an external routine using flat physical addresses.  Has the
 * capability to pass in and out register and stack contents, relocate
 * stack to a new address, use a trampoline routine on the stack etc.
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
#define EC_SIG_TRAMPOLINE_RET ( EC_SIG_SIG1 - 8 )
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
	 * trampoline_return	%ebp + EC_SIG_TRAMPOLINE_RET ( = %ebp - 8 )
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
	 * trampoline_return : 8 bytes reserved at the end of
	 * ext_params.  The far return address is stored at the end of
	 * ext_params when a trampoline is used, enabling the
	 * trampoline to discard the return address at the head of the
	 * stack if necessary.  Note that the return address is stored
	 * to follow immediately from the end of the trampoline code.
	 * 
	 * ext_params : Parameters to pass on stack to external call.
	 * "length" indicates the length of this data, which may not
	 * be a multiple of 4.  The bottom of ext_params is kept
	 * aligned, i.e. there is padding at the top.
	 *
	 * addresses : return and call addresses.  Always 4 dwords.
	 * For a near call, these are (return address, flat code
	 * segment, call address, 0); for a far call (flat code
	 * segment, return address, call segment, call address).
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
	movl	$FLAT_CODE_SEG, %ecx
	pushl	%ecx			/* Far return address */
	pushl	%eax

	/* Save default addresses for a 'near' call */
	pushl	%eax			/* Near return address */
	pushl	%ecx			/* Far call segment */
	pushl	EC_OFFSET_CALLADDR(%ebp) /* Call address */
	xorl	%eax, %eax
	pushl	%eax			/* 0 (indicating 'near' call) */

	/* Save segment registers.  Order matches the seg_regs_t
	 * struct in callbacks.h
	 */
	pushw	%ax			/* %cs = 0 => "near call" */
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
	movl	EC_OFFSET_ARG_LIST(%ebp), %ebx
	
	/* Ensure that movsb etc work as expected */
	cld

	/* Loop to process parameters to _ext_call */
process_ec_params:
	movl	0(%ebx), %eax		/* %eax = type */
	addl	$4, %ebx
	cmpl	$EXTCALL_END_LIST, %eax
	je	process_ec_params_done
	cmpl	$EXTCALL_NONE, %eax
	je	process_ec_params
	cmpl	$EXTCALL_REGISTERS, %eax
	je	process_ec_params_registers
	cmpl	$EXTCALL_SEG_REGISTERS, %eax
	je	process_ec_params_seg_registers
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
process_ec_params_seg_registers:
	movl	0(%ebx), %esi		/* %esi = &seg_registers */
	addl	$4, %ebx
	movl	$NUM_SEG_REGS, %ecx
	leal	EC_OFFSET_SEG_REGS(%esp), %edi
	rep movsw
	/* Check code segment register, set near/far call addresses */
	leal	ec_return, %esi		/* %esi = return address */
	addl	virt_offset, %esi	/* %esi = physical return address */
	movl	EC_OFFSET_CALLADDR(%ebp), %edi /* %edi = call address */
	movzwl	(EC_OFFSET_SEG_REGS+10)(%esp), %eax /* %eax = '%cs' */
	movl	$FLAT_CODE_SEG, %ecx	/* %ecx = flat code seg */
	testl	%eax, %eax
	jne	1f
	/* 'near' call */
	movl	%eax, (EC_OFFSET_ADDRESSES+0)(%esp)	/* zero */
	movl	%edi, (EC_OFFSET_ADDRESSES+4)(%esp)	/* call address */
	movl	%ecx, (EC_OFFSET_ADDRESSES+8)(%esp)	/* flat code segment */
	movl	%esi, (EC_OFFSET_ADDRESSES+12)(%esp)	/* return address */
	jmp	process_ec_params
1:	/* far call */
	movl	%edi, (EC_OFFSET_ADDRESSES+0)(%esp)	/* call address */
	movl	%eax, (EC_OFFSET_ADDRESSES+4)(%esp)	/* call segment */
	movl	%esi, (EC_OFFSET_ADDRESSES+8)(%esp)	/* return address */
	movl	%ecx, (EC_OFFSET_ADDRESSES+12)(%esp)	/* return segment */
	jmp	process_ec_params
process_ec_params_stack:
	/* Move data currently on stack down to accommodate new data */
	movl	EC_SIG_LENGTH(%ebp), %ecx /* %ecx = current data length */
	movl	4(%ebx), %edx		/* %edx = extra data length */
	addl	$EC_OFFSET_PARAMS, %ecx	/* %ecx = total data len to copy */
	movl	%esp, %esi		/* Source location */
	leal	EC_SIG_PARAMS_TOP(%ebp), %edi
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

	/* Switch to flat physical stack segment, adjust %esp and %ebp */
	movl	$FLAT_DATA_SEG, %eax
	movl	virt_offset, %ebx
	movw	%ax, %ss
	addl	%ebx, %esp
	addl	%ebx, %ebp
	/* %ax = stack segment */
	
	/* Copy stack to new location indicated by stack_ptr, reset
	 * %ss:esp to point to it.
	 */
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
	leal	(-EC_SIGBLOCK_LENGTH)(%edi), %ebp /* Set new %ebp */

	/* Check for use of trampoline, edit call address as
	 * appropriate.
	 */
	movl	EC_SIG_TRAMPOLINE(%ebp), %eax /* %eax = (trampoline-params) */
	testl	%eax, %eax
	je	2f			/* No trampoline */
	/* Align trampoline return address to end of trampoline code */
	leal	EC_SIG_TRAMPOLINE_RET(%ebp), %esi
	leal	EC_OFFSET_PARAMS(%esp), %edi
	addl	EC_SIG_LENGTH(%ebp), %edi
	movl	$8, %ecx
	rep ss movsb
	/* Set trampoline address as call address */
	addl	$EC_OFFSET_PARAMS, %eax	/* %eax = (trampoline-%esp) */
	addl	%esp, %eax		/* %eax = trampoline address */
	movl	(EC_OFFSET_ADDRESSES+0)(%esp), %ecx
	testl	%ecx, %ecx
	jne	1f
	/* 'near' call */
	movl	%eax, (EC_OFFSET_ADDRESSES+4)(%esp)
	jmp	2f
1:	/* far call */
	movl	%eax, (EC_OFFSET_ADDRESSES+0)(%esp)
2:

	/* Restore flags. */
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
	cmpl	$0, 0(%esp)
	jne	1f
	addl	$4, %esp		/* Near call: skip the zero */
1:	lret				/* Do the calculated ljmp */

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
	je	process_ecr_params_done
	cmpl	$EXTCALL_NONE, %eax
	je	process_ecr_params
	cmpl	$EXTCALL_REGISTERS, %eax
	je	process_ecr_params_registers
	cmpl	$EXTCALL_SEG_REGISTERS, %eax
	je	process_ecr_params_seg_registers
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
 * real-mode code).  _prot_to_real does not automatically re-enable
 * interrupts; this is to allow for the potential of using Etherboot
 * code as an ISR.  _real_to_prot does automatically disable
 * interrupts, since we don't have a protected-mode IDT.
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
#define RIC_OFFSET_CALLADDR ( RIC_PRESERVE )
#define RIC_OFFSET_CALLADDR_E ( RIC_OFFSET_CALLADDR + 8 )
#define RIC_OFFSET_CONTADDR ( RIC_OFFSET_CALLADDR_E )
#define RIC_OFFSET_CONTADDR_E ( RIC_OFFSET_CONTADDR + 8 )
#define RIC_OFFSET_OPCODE ( RIC_OFFSET_CONTADDR_E )
#define RIC_OFFSET_OPCODE_E ( RIC_OFFSET_OPCODE + 4 )
#define RIC_INSERT_LENGTH ( RIC_OFFSET_OPCODE_E - RIC_OFFSET_CALLADDR )

/* Slightly messy that we need to know this */
#define RIC_R2P_INSERT_LENGTH ( NUM_SEG_REGS * 2 )

#define RIC_OFFSET_R2P_INSERTED ( RIC_OFFSET_OPCODE_E )
#define RIC_OFFSET_R2P_INSERTED_E ( RIC_OFFSET_R2P_INSERTED + \
				     RIC_R2P_INSERT_LENGTH )
#define RIC_OFFSET_LONG_PRESERVE ( RIC_OFFSET_R2P_INSERTED_E )
#define RIC_OFFSET_LONG_PRESERVE_E ( RIC_OFFSET_LONG_PRESERVE + 4 )
#define RIC_OFFSET_RETADDR ( RIC_OFFSET_LONG_PRESERVE_E )
#define RIC_OFFSET_RETADDR_E ( RIC_OFFSET_RETADDR + 4 )
#define RIC_OFFSET_ORIG_OPCODE ( RIC_OFFSET_RETADDR_E )
	
	.code16
_real_in_call:
	/* Expand near return address to far return address
	 */
	pushw	%ax		/* padding */
	pushfw
	pushw	%bp
	pushw	%ax
	movw	%sp, %bp
	movw	%cs, %ax
	xchgw	%ax, 8(%bp)
	movw	%ax, 6(%bp)
	popw	%ax
	popw	%bp
	popfw
	/* Fall through to _real_call_far */
	
	.code16
_real_in_call_far:
	/* Store flags and pad; this is the "LONG_PRESERVE" from above */
	pushfw
	pushw	%ax

	/* Call to _real_to_prot */
	call _real_to_prot_core

	/* Continuation point */
1:	.code32

	/* We're in protected mode with flat physical addresses and
	 * all the parameters are on the stack: set up continuation
	 * address and 'ljmp' into _in_call_far.
	 */

	/* Allow space for expanded stack */
	subl	$RIC_INSERT_LENGTH, %esp
	
	/* Store temporary registers */
	pushl	%ebp
	pushl	%eax

	/* Copy opcode, set EB_CALL_FROM_REAL_MODE.  Copy it because
	 * _in_call() and i386_in_call() expect it at a fixed
	 * position, not as part of the va_list.
	 */
	movl	RIC_OFFSET_ORIG_OPCODE(%esp), %eax
	orl	$EB_CALL_FROM_REAL_MODE, %eax
	movl	%eax, RIC_OFFSET_OPCODE(%esp)
	
	/* Set up call and return addresses */
	movl	$(r2p_pmcs-r2p_gdt), %eax	/* Flat physical CS */
	movl	%eax, (RIC_OFFSET_CALLADDR+4)(%esp)
	movl	%eax, (RIC_OFFSET_CONTADDR+4)(%esp)
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp			/* %ebp = offset */
	movl	rm_etherboot_location(%ebp), %eax  /* Etherboot phys addr */
	subl	$_text, %eax
	addl	$_in_call_far, %eax		/* _in_call_far phys addr */
	movl	%eax, (RIC_OFFSET_CALLADDR+0)(%esp)
	leal	2f(%ebp), %eax			/* continuation address */
	movl	%eax, (RIC_OFFSET_CONTADDR+0)(%esp)
	
	/* Restore temporary registers */
	popl	%eax
	popl	%ebp

	/* Call to _in_call_far, will return via _prot_to_real */
	lret

2:	/* Continuation point */
	/* Remove opcode from stack */
	addl	$4, %esp
	/* Return via _prot_to_real */
	jmp	_prot_to_real_core

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
 * Case (a): (_prot_to_real)
 *
 * Parameters:
 *   32-bit far pmode return address
 *   seg_regs_t rm_seg_regs : structure for real-mode segment register values.
 *
 * This routine will strip the far return address and segment
 * registers from the stack.  The return address is effectively
 * discarded.  Return to Etherboot is via _real_to_prot, which will
 * use the duplicate return address stored at the end of the
 * trampoline code.  This allows the real-mode code to use the
 * parameters on the stack (including passing them to external
 * routines) without having to take the Etherboot return address into
 * account.
 *
 * Case (b): (_prot_to_real_core)
 *   seg_regs_t rm_seg_regs : structure for real-mode segment register values.
 *   padding (2 bytes)
 *   flags
 *   16-bit far rmode return address
 *
 * This routine will strip the segment registers from the stack and
 * then return to the specified address.
 ****************************************************************************
 */

#define P2R_PRESERVE (12)
#define P2R_OFFSET_SEG_REGS ( P2R_PRESERVE )
#define P2R_OFFSET_SEG_REGS_E ( P2R_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )

	.globl _prot_to_real
	.code32
_prot_to_real:
	addl	$8, %esp		/* Skip return address */
_prot_to_real_core:	
	/* Store temporary registers */
	pushl	%ebp
	pushl	%eax
	pushl	%ebx
	
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

	/* Restore registers */
	popl	%ebx
	popl	%eax
	popl	%ebp

	/* Set up segment registers with specified values */
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	addw	$4, %sp			/* skip ss, cs (already set) */	

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
	 * real-mode code, we now have stored flags and a 16-bit
	 * real-mode far return address on the stack.
	 */
	addw	$2, %sp		/* skip padding */
	popfw			/* restore flags (incl. interrupt status) */
	lret			/* Back to real-mode caller */
	
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
 * Case (a): (_real_to_prot)
 *
 * Parameters:
 *   none
 *
 * This routine will push a seg_regs_t structure containing the
 * real-mode segment registers onto the stack before returning to the
 * return address stored at the end of the trampoline code.
 *
 * Case (b): (_real_to_prot_core)
 *
 * Parameters:
 *   none
 *
 * This routine will push a seg_regs_t structure containing the
 * real-mode segment registers onto the stack and return to the caller.
 *   
 ****************************************************************************
 */

#define R2P_PRESERVE ( 12 )
#define R2P_OFFSET_RETADDR ( R2P_PRESERVE )
#define R2P_OFFSET_RETADDR_E ( R2P_OFFSET_RETADDR + 4 )
#define R2P_OFFSET_SEG_REGS ( R2P_OFFSET_RETADDR_E )
#define R2P_OFFSET_SEG_REGS_E ( R2P_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define R2P_ORIG_OFFSET_RETADDR_E ( R2P_OFFSET_SEG_REGS_E )
#define R2P_ORIG_OFFSET_RETADDR ( R2P_ORIG_OFFSET_RETADDR_E - 2 )
#define R2P_INSERT_LENGTH ( R2P_ORIG_OFFSET_RETADDR - R2P_OFFSET_RETADDR )

	.globl _real_to_prot
	.globl _ereal_to_prot
	
	.code16		
_real_to_prot:
	call _real_to_prot_core
	.code32
	/* Set up far return address on stack using address
	 * stored at end of trampoline.
	 */
	pushl	$(r2p_pmcs - r2p_gdt)	/* physical segment */
	pushl	%eax			/* padding */
	pushl	%ebp			/* preserve ebp */
	call	1f
1:	popl	%ebp
	movl	%cs:(_real_to_prot_end-1b)(%ebp), %ebp
	movl	%ebp, 4(%esp)
	popl	%ebp			/* restore ebp */

	/* Return to Etherboot */
	lret

	.code16
_real_to_prot_core:
	/* Store segment registers, skipping %cs.  They end up in the
	 * right place, because there's currently exactly two bytes on
	 * the stack: the near return address.  We'll store %cs later.
	 */
	pushw	%ss
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs

	/* Allow space for 32-bit return address */
	pushw	%ax
	pushw	%ax
		
	/* Store interrupt status and temporary registers */
	pushl	%ebp
	pushl	%eax
	pushl	%ebx

	/* Disable interrupts */
	cli
	
	/* Expand return address from 16- to 32-bit, move it down to
	 * below the segment register dump.  Fill in %cs in the
	 * segment register dump.
	 */
	movw	%sp, %bp
	xorl	%eax, %eax
	movw	%cs, %ax
	xchgw	(R2P_ORIG_OFFSET_RETADDR)(%bp), %ax
	movl	%eax, (R2P_OFFSET_RETADDR)(%bp)

	/* Calculate 32-bit continuation address and push onto stack */
	xorl	%eax, %eax
	movw	%cs, %ax
	shll	$4, %eax		/* %eax = %cs << 4 */
	addl	%eax, R2P_OFFSET_RETADDR(%bp) /* Adjust local ret. addr */
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
	popl	%ebp

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
