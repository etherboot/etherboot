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
#define EC_SIGBLOCK_LENGTH (12)
#define EC_OFFSET_CALLPRESERVE ( EC_OFFSET_SIGBLOCK + EC_SIGBLOCK_LENGTH )
#define EC_CALLPRESERVE_LENGTH (16)
#define EC_OFFSET_RETADDR ( EC_OFFSET_CALLPRESERVE + EC_CALLPRESERVE_LENGTH )
#define EC_RETADDR_LENGTH (4)
#define EC_OFFSET_ARGUMENTS ( EC_OFFSET_RETADDR + EC_RETADDR_LENGTH )

/* Offsets from current %esp */
/* Outgoing structure */
#define EC_OFFSET_REGISTERS 0
#define EC_OFFSET_SEG_REGS ( EC_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
#define EC_OFFSET_ADDRESSES ( EC_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define EC_OFFSET_PARAMS ( EC_OFFSET_ADDRESSES + 16 )
/* Return structure */
#define ECR_OFFSET_REGISTERS 0
#define ECR_OFFSET_SEG_REGS ( ECR_OFFSET_REGISTERS + ( NUM_PUSHA_REGS * 4 ) )
#define ECR_OFFSET_PARAMS ( ECR_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )

	.text
	.arch i386
	.code32

/* _ext_call : call to an external routine.
 */
	.globl _ext_call
_ext_call:
	/* Save registers required to return to C code */
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	/* We set up a stack that looks like this:
	 * 
	 * EC_SIGNATURE
	 * length
	 * EC_SIGNATURE
	 * ext_params		%esp + EC_OFFSET_PARAMS
	 * addresses		%esp + EC_OFFSET_ADDRESSES
	 * seg_registers	%esp + EC_OFFSET_SEG_REGS
	 * registers		%esp + EC_OFFSET_REGISTERS ( = %esp + 0 )
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
	 * %ebp is kept pointing to the bottom EC_SIGNATURE, so that
	 * length is available at 4(%ebp) throughout.
	 *
	 * %ebx is used to point to the current argument in our
	 * argument list as we process them.
	 */

	/* Create signature block */
	movl	$EC_SIGNATURE, %eax
	xorl	%ecx, %ecx
	pushl	%eax		/* Set up signature and initial length */
	pushl	%ecx
	pushl	%eax
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
	pushw	%cs
	pushw	%ss
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs

	/* Save registers.  (Current register contents are fairly
	 * meaningless, but using pushal is the most efficient way to
	 * allocate the stack space.
	 */
	pushal

	/* Set %ebx to point to first argument in list */
	leal	(EC_OFFSET_ARGUMENTS+4)(%ebp), %ebx
	
	/* Mask interrupts around non-reentrant code */
	cli

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
	cmpl	$EXTCALL_GDT, %eax
	je	process_ec_params_gdt
	cmpl	$EXTCALL_STACK, %eax
	je	process_ec_params_stack
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
	/* Check code segment register, set near/far call addresses */
	leal	ec_return, %ecx		/* %ecx = return address */
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
	movl	0(%ebx), %eax		/* %ebx = &gdt */
	addl	$4, %ebx
	lgdt	(%eax)
	/* Don't reset the segment registers yet */
	jmp	process_ec_params
process_ec_params_stack:
	/* Move data currently on stack down to accommodate new data */
	movl	4(%ebp), %ecx		/* %ecx = current data length */
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
	addl	%edx, 4(%ebp)		/* Update length */
	addl	$8, %ebx
	jmp	process_ec_params
process_ec_params_done:
	/* Stack is all set up as described above.  Set up the
	 * register values and jump to the function.
	 */

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
	 */
	cld			/* Just in case */

	/* Basic strategy is to set up a structure similar to the one
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

	/* Point ds and es to the stack, for ease of signature
	 * scanning and re-aligning.
	 */
	pushw	%ss
	popw	%ds
	pushw	%ss
	popw	%es
	
	/* Locate signature.  It is guaranteed to be on a dword
	 * boundary, but %ebp may not be.
	 */
	movl	$EC_SIGNATURE, %eax
	leal	ECR_OFFSET_PARAMS(%esp), %edi /* Returned %esp */
	andl	$~0x3, %edi	/* Round down to align */
	xorl	%ecx, %ecx
	decl	%ecx		/* Scan indefinitely */
1:	repne scasl		/* Search for signature */
	cmpl	4(%edi), %eax	/* Check second signature */
	jne	1b		/* Continue searching if not found */
	subl	$4, %edi
	movl	%edi, %ebp
	/% ebp now points to signature, as before call */

	/* Calculate length of returned parameters and store */
	movl	%ebp, %ecx	/* &(signature) */
	leal	ECR_OFFSET_PARAMS(%esp), %edx /* Returned %esp */
	subl	%edx, %ecx
	movl	%ecx, 4(%ebp)	/* Store length */

	/* Align stack by shifting down */
	movl	%esp, %esi
	andl	$~0x3, %esp
	movl	%esp, %edi
	rep movsb		/* Shift stack down */
	/* Stack aligned, length set as before, structures in place */

	/* Reload GDT at this point, copy back stack and reset segment
	 * registers?  Should be clear to do it now; we've got the
	 * signature block in ebp so we know where to find anything
	 * that was set up on callout, and we have the stack structure
	 * in the shape we want it.  Signature block will have to
	 * store a "reload GDT" flag.
	 */

	/* Reload segment registers.  cs and ss dealt with by "reload
	 * GDT" code.
	 */
	movw	$KERN_DATA_SEG, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	/* %ebx now points to first parameter in list, as before */
	leal	(EC_OFFSET_ARGUMENTS+4)(%ebp), %ebx

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
	addl	$4, %ebx
	jmp	process_ecr_params
process_ecr_params_stack:
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
	
	.globl demo_extcall
demo_extcall:
	movl	4(%esp), %eax
	addl	8(%esp), %eax
	addl	12(%esp), %eax
	addl	16(%esp), %eax
	movl	%ebx, %ecx
	ret
	
