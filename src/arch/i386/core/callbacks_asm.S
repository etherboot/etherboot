/* Callout/callback interface for Etherboot
 *
 * This file provides the mechanisms for making calls from Etherboot
 * to external programs and vice-versa.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "callbacks.h"
#define PUSHA_NUM_REGS 8
#define NUM_SEG_REGS 6
#define EC_FIXED_STACK ( PUSHA_NUM_REGS * 4 )

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

	/* EC_FIXED_STACK should contain the length of data pushed
	 * onto the stack between HERE...
	 */
	
	movl	%esp, %ebp
	addl	$24, %ebp	/* %ebp points to first parameter in list */

	/* Mask interrupts around non-reentrant code */
	cli

	/* Save defaults */
	xorl	%eax, %eax
	movw	%ax, ec_save_cs	/* 0 in cs => near call */
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ss
	pushw	%ds
	popw	ec_save_ds
	popw	ec_save_ss
	popw	ec_save_es
	popw	ec_save_fs
	popw	ec_save_gs
	pushal
	xorl	%eax, %eax
	movl	%eax, ec_stack_size
	cld


	/* ... and HERE (see above) */
#if 1
	
	/* Stack now contains the ec_save_* for all other registers.
	 * ec_stack_size records the amount of data we've copied to
	 * below %esp: the "extra stack"
	 *  
	 * DO NOT USE THE STACK AFTER THIS POINT
	 */

	/* Loop to process parameters to _ext_call */
process_ec_params:
	movl	0(%ebp), %eax		/* %eax = type */
	addl	$4, %ebp
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
	movl	0(%ebp), %esi		/* %esi = &registers */
	addl	$4, %ebp
	movl	$PUSHA_NUM_REGS , %ecx
	movl	%esp, %edi		/* %edi = &(pusha struct on stack) */
	rep movsl
	jmp	process_ec_params
process_ec_params_seg_registers:
	movl	0(%ebp), %esi		/* %esi = &seg_registers */
	addl	$4, %ebp
	movl	$NUM_SEG_REGS, %ecx
	leal	ec_save_seg_regs, %edi	/* %edi = &ec_save_seg_regs */
	rep movsw
	jmp	process_ec_params
process_ec_params_gdt:
	movl	0(%ebp), %ebx		/* %ebx = &gdt */
	addl	$4, %ebp
	lgdt	(%ebx)
	/* Don't reset the segment registers yet */
	jmp	process_ec_params
process_ec_params_stack:
	/* Move data currently on stack down to accommodate new data */
	movl	ec_stack_size, %ecx
	movl	%esp, %esi
	subl	%ecx, %esi		/* %esi = current stack bottom */
	movl	4(%ebp), %ebx		/* New data length */
	movl	%esi, %edi
	subl	%ebx, %edi		/* %edi = new stack bottom */
	rep movsb			/* Move data out of the way */
	movl	%ebx, %ecx		/* New data length */
	movl	0(%ebp), %esi		/* New data source */
	rep movsb
	addl	$8, %ebp
	addl	%ebx, ec_stack_size
	jmp	process_ec_params
process_ec_params_done:
#endif
	/* Desired register contents are now at front of stack, ready
	 * for a popal.  Just in front of the stack is the "extra
	 * stack", the data we want to pass on the stack to the
	 * external function.  ec_stack_size records the length of
	 * this data.  ec_save_seg_regs contains the desired segment
	 * registers (including cs).  The GDT has been loaded.
	 */
	/* Restore the original ebp */
	movl	%esp, %ebp
	addl	$(EC_FIXED_STACK + 12), %ebp
	/* Align the "extra stack" and move %esp beneath it. */
	movl	ec_stack_size, %ecx
	movl	%esp, %esi
	subl	%ecx, %esi
	movl	%esi, %edi
	andl	$~0x3, %edi
	movl	%edi, %esp
	rep movsb
	/* We can now use the stack again */
	/* Set up the return and jump address */
	movzwl	ec_save_cs, %eax
	testl	%eax,%eax		/* ec_save_cs = 0 => near call */
	je	1f
	pushl	%cs			/* Far return segment */
1:	leal	ec_return, %ebx
	pushl	%ebx			/* Near return address */
	je	1f
	pushl	%eax			/* Far call segment */
1:	pushl	8(%ebp)			/* Near/far call address */
	/* Restore registers */
	pushfl
	addl	ec_stack_size, %esp	/* Skip back past jump and return  */
	addl	$(20+3), %esp		/* addresses and the "extra stack" */
	andl	$~0x3, %esp
	popal
	subl	$(EC_FIXED_STACK + 20), %esp
	subl	ec_stack_size, %esp
	andl	$~0x3, %esp		/* %esp back to where it was */
	popfl
	/* Load up segment registers.  cs gets loaded by the "ljmp" */
	/* FIXME: handle SS */
	pushw	ec_save_ds
	pushw	ec_save_es
	pushw	ec_save_fs
	pushw	ec_save_gs
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	/* Zero flag still indicates near/far call */
	jne	2f
1:	/* Near "jmpl" */
	ret
2:	/* Far "ljmp" */
	lret

	/* Return point */
ec_return:

/*99:	jmp 99b */
	
	/* temporary for testing, very non-reentrant  */
	addl	ec_stack_size, %esp
	addl	$((PUSHA_NUM_REGS * 4)+3), %esp
	andl	$~0x3, %esp
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
	
	.bss
	.balign 4
ec_save_seg_regs:	
ec_save_cs:	.space 2
ec_save_ds:	.space 2
ec_save_ss:	.space 2
ec_save_es:	.space 2
ec_save_fs:	.space 2
ec_save_gs:	.space 2
ec_stack_size:	.space 4

	.text
	.globl demo_extcall
demo_extcall:
	movl	4(%esp), %eax
	addl	8(%esp), %eax
	addl	12(%esp), %eax
	addl	16(%esp), %eax
	movl	%ebx, %ecx
	ret
	