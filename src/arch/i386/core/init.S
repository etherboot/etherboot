#include "callbacks.h"
	.equ CR0_PE, 1
	
	.text
	.arch i386

#undef CODE16
#if defined(PCBIOS) || defined(TAGGED_IMAGE)
#define	CODE16
#endif

/* We have two entry points: "conventional" (at the start of the file)
 * and "callback" (at _entry, 2 bytes in).  The "callback" entry
 * should be used if the caller wishes to provide a specific opcode.
 * It is equivalent to a call to in_call.  Using the "conventional"
 * entry point is equivalent to using the "callback" entry point with
 * an opcode of EB_OPCODE_MAIN.
 *
 * Both entry points can be called in either 16-bit real or 32-bit
 * protected mode with flat physical addresses.  We detect which mode
 * the processor is in and call either in_call or rm_in_call as
 * appropriate.  Note that the mode detection code must therefore be
 * capable of doing the same thing in either mode, even though the
 * machine code instructions will be interpreted differently.
 *
 * The decompressor will be invoked if necessary to decompress
 * Etherboot before attempting to jump to it.
 */

/******************************************************************************
 * Entry points and mode detection code
 ******************************************************************************
 */

	.code32
/* "Conventional" entry point: caller provides no opcode */
	.globl	_start
_start:
	/* Set flag to indicate conventional entry point used */
	pushl	$0			/* "pushw $0" in 16-bit code */
	/* Fall through to "callback" entry point */
	
/* "Callback" entry point */
	.globl	_entry
_entry:
	
#ifdef CODE16
	/* CPU mode detection code */
	pushl	%eax			/* "pushw %ax" in 16-bit code */
	pushw	%ax			/* "pushl %eax" in 16-bit code */
	movl	%cr0, %eax		/* Set protected mode bit */
	testb	$CR0_PE, %al
	popw	%ax			/* "popl %eax" in 16-bit code */
	popl	%eax			/* "popw %eax" in 16-bit code */
	jz	rmode
#endif /* CODE16 */

/******************************************************************************
 * Entered in protected mode
 ******************************************************************************
 */
		
	.code32
pmode:
	cmpl	$0, 0(%esp)		/* Conventional entry point used? */
	jne	1f
	/* Entered via conventional entry point: set up stack */
	xchgl	%eax, 4(%esp)		/* %eax = return addr, store %eax */
	movl	%eax, 0(%esp)		/* 0(%esp) = return address */
	movl	$(EB_OPCODE_MAIN|EB_USE_INTERNAL_STACK|EB_SKIP_OPCODE), %eax
	xchgl	%eax, 4(%esp)		/* 4(%esp) = opcode, restore %eax */
1:
	/* jmp in_call */
	ret

/******************************************************************************
 * Entered in real mode
 ******************************************************************************
 */
	
#ifdef CODE16
	.code16
rmode:
	pushw	%ax			/* Padding */
	pushw	%bp
	movw	%sp, %bp
	cmpw	$0, 4(%bp)		/* Conventional entry point used? */
	jne	1f
	/* Entered via contentional entry point: set up stack */
	pushl	%eax
	movw	10(%bp), %ax
	movw	%ax, 6(%bp)		/* Move return address down */
	movl	$(EB_OPCODE_MAIN|EB_USE_INTERNAL_STACK|EB_SKIP_OPCODE), 8(%bp)
	popl	%eax
	popw	%bp
	jmp	2f
1:	/* Entered via callback entry point: do nothing */
	popw	%bp
	popw	%ax
2:	

	/* jmp rm_in_call */
	ret
#endif /* CODE16 */
	
