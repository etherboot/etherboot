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
	/* Fall through to _real_in_call_far */
	
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
 *   32-bit pmode near return address
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
 *   16-bit rmode far return address
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
	addl	$4, %esp		/* Skip return address */
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
	pushl	%eax			/* address will go here */
	pushl	%ebp			/* preserve ebp */
	call	1f
1:	popl	%ebp
	movl	%cs:(_real_to_prot_end-1b)(%ebp), %ebp
	movl	%ebp, 4(%esp)
	popl	%ebp			/* restore ebp */

	/* Return to Etherboot */
	ret

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
