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

#define NUM_PUSHA_REGS (8)
#define NUM_SEG_REGS (6)

#define EC_SIGNATURE ( 0x43747845 ) /* "ExtC" */

/* Offsets from base of signature block */
#define EC_OFFSET_SIGBLOCK (0)
#define EC_SIG_SIG1 (0)
#define EC_SIG_LENGTH ( EC_SIG_SIG1 + 4 )
#define EC_SIG_STACK ( EC_SIG_LENGTH + 4 )
#define EC_SIG_SIG2 ( EC_SIG_STACK + 4 )
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

	.text
	.arch i386
	.code32

/* _v_ext_call : call to an external routine.
 */
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
	 * stack_ptr		%ebp + EC_SIG_STACK
	 * length		%ebp + EC_SIG_LENGTH
	 * EC_SIGNATURE		%ebp + EC_SIG_SIG1 ( = %ebp + 0 )
	 * ext_params		%esp + EC_OFFSET_PARAMS
	 * addresses		%esp + EC_OFFSET_ADDRESSES
	 * seg_registers	%esp + EC_OFFSET_SEG_REGS
	 * registers		%esp + EC_OFFSET_REGISTERS
	 * flags		%esp + EC_OFFSET_FLAGS ( = %esp + 0 )
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
	 * 0, 0); for a far call (return segment, return address, call
	 * segment, call address ).
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
	pushl	%eax		/* 0, 0 (indicating near call) */
	pushl	%eax

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
	movl	%edx, (EC_OFFSET_ADDRESSES+8)(%esp)	/* call address */
	movl	%ecx, (EC_OFFSET_ADDRESSES+12)(%esp)	/* return address */
	movl	%eax, (EC_OFFSET_ADDRESSES+4)(%esp)	/* zero */
	movl	%eax, (EC_OFFSET_ADDRESSES+0)(%esp)	/* zero */
	jmp	process_ec_params
1:	/* far call */
	movl	%eax, (EC_OFFSET_ADDRESSES+4)(%esp)	/* call segment */
	movl	%edx, (EC_OFFSET_ADDRESSES+0)(%esp)	/* call address */
	pushl	%cs
	popl	%eax
	movl	%eax, (EC_OFFSET_ADDRESSES+12)(%esp)	/* return segment */
	movl	%ecx, (EC_OFFSET_ADDRESSES+8)(%esp)	/* return address */
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
	mov	%ax, %ss		/* Set new %ss */

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
	cmpl	$0, 0(%esp)
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
	
	.globl demo_extcall
demo_extcall:
	movl	4(%esp), %eax
/*	addl	8(%esp), %eax
	addl	12(%esp), %eax
	addl	16(%esp), %eax */
	movl	%ebx, %ecx
	mov	$0x18, %dx
	mov	%dx, %ds
	mov	%dx, %es
	popl	%ebx			/* return address */
	pushw	$0xabcd
	pushw	$0x5678
	pushw	$0x1234
	pushw	%cs
	pushl	%ebx
	ret
	
