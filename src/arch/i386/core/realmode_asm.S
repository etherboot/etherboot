/* Real-mode interface: assembly-language portions.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "realmode.h"
#include "callbacks.h"

#undef CODE16
#if defined(PCBIOS) || defined(TAGGED_IMAGE)
#define	CODE16
#endif

#ifdef CODE16
	
#define BOCHSBP xchgw %bx,%bx

#define NUM_PUSHA_REGS (8)
#define NUM_SEG_REGS (6)

	.text
	.arch i386
	.section ".text16", "ax", @progbits
	.code16
	
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
 *   16-bit real-mode near/far return address (implicit from [l]call
 *   to routine) Other parameters as for _in_call_far().
 *
 * This routine will convert the 16-bit real-mode far return address
 * to a 32-bit real-mode far return address, switch to protected mode
 * using _real_to_prot and call in to _in_call_far.
 ****************************************************************************
 */

#define RIC_PRESERVE ( 8 )
#define RIC_OFFSET_CALLADDR ( RIC_PRESERVE )
#define RIC_OFFSET_CALLADDR_E ( RIC_OFFSET_CALLADDR + 4 )
#define RIC_OFFSET_CONTADDR ( RIC_OFFSET_CALLADDR_E )
#define RIC_OFFSET_CONTADDR_E ( RIC_OFFSET_CONTADDR + 4 )
#define RIC_OFFSET_OPCODE ( RIC_OFFSET_CONTADDR_E )
#define RIC_OFFSET_OPCODE_E ( RIC_OFFSET_OPCODE + 4 )
#define RIC_OFFSET_SEG_REGS ( RIC_OFFSET_OPCODE_E )
#define RIC_OFFSET_SEG_REGS_E ( RIC_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define RIC_OFFSET_PAD ( RIC_OFFSET_SEG_REGS_E )
#define RIC_OFFSET_PAD_E ( RIC_OFFSET_PAD + 2 )
#define RIC_OFFSET_FLAGS ( RIC_OFFSET_PAD_E )
#define RIC_OFFSET_FLAGS_E ( RIC_OFFSET_FLAGS + 2 )
#define RIC_OFFSET_RETADDR ( RIC_OFFSET_FLAGS_E )
#define RIC_OFFSET_RETADDR_E ( RIC_OFFSET_RETADDR + 4 )
#define RIC_OFFSET_ORIG_OPCODE ( RIC_OFFSET_RETADDR_E )
#define RIC_INSERT_LENGTH ( RIC_OFFSET_OPCODE_E - RIC_OFFSET_CALLADDR )
	
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
	
_real_in_call_far:
	/* Store flags and pad */
	pushfw
	pushw	%ax

	/* Store segment registers.  Order matches that of seg_regs_t */
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ds
	pushw	%ss
	pushw	%cs

	/* Switch to protected mode */
	call _real_to_prot
	.code32

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
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp			/* %ebp = offset */
	movl	rm_etherboot_location(%ebp), %eax  /* Etherboot phys addr */
	subl	$_text, %eax
	addl	$_in_call, %eax			/* _in_call phys addr */
	movl	%eax, RIC_OFFSET_CALLADDR(%esp)
	leal	2f(%ebp), %eax			/* continuation address */
	movl	%eax, RIC_OFFSET_CONTADDR(%esp)
	
	/* Restore temporary registers */
	popl	%eax
	popl	%ebp

	/* Call to _in_call */
	ret

2:	/* Continuation point */
	addl	$4, %esp			/* Remove opcode from stack */
	call	_prot_to_real			/* Return to real mode */
	/* Note: the first two words of our segment register store
	 * happens to be exactly what we need to pass as parameters to
	 * _prot_to_real.
	 */
	.code16
	popw	%ds				/* Restore segment registers */
	popw	%ds				/* (skip cs&ss since these   */
	popw	%ds				/* have already been set by  */
	popw	%es				/* _prot_to_real	     */
	popw	%fs
	popw	%gs
	addw	$2, %sp				/* skip pad */
	popfw					/* Restore interrupt status */
	lret					/* Back to caller */

/****************************************************************************
 * rm_etherboot_location: the current physical location of Etherboot.
 * Needed so that real-mode callback routines can locate Etherboot.
 ****************************************************************************
 */
	.globl rm_etherboot_location
rm_etherboot_location:	.long 0
		
/****************************************************************************
 * _prot_to_real_prefix
 *
 * Trampoline fragment.  Switch from 32-bit protected mode with flat
 * physical addresses to 16-bit real mode.  Store registers in the
 * trampoline for restoration by _real_to_prot_suffix.  Switch to
 * stack in base memory.
 ****************************************************************************
 */
		
	.globl _prot_to_real_prefix
	.code32
_prot_to_real_prefix:
	/* Registers to preserve */
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp

	/* Calculate offset */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp		/* %ebp = offset for labels in p2r*/

	/* Preserve registers and return address in r2p_params */
	movl	p2r_r2p_params(%ebp), %ebx
	subl	$r2p_params, %ebx	/* %ebx = offset for labels in r2p */
	popl	r2p_ebp(%ebx)
	popl	r2p_edi(%ebx)
	popl	r2p_esi(%ebx)
	popl	r2p_ebx(%ebx)
	popl	r2p_ret_addr(%ebx)
	movl	%esp, r2p_esp(%ebx)

	/* Switch stacks */
	movl	p2r_esp(%ebp), %esp

	/* Switch to real mode */
	pushl	p2r_segments(%ebp)
	call	_prot_to_real
	.code16
	addw	$4, %sp
		
	/* Fall through to next trampoline fragment */
	jmp	_prot_to_real_prefix_end
	
/****************************************************************************
 * _prot_to_real
 *
 * Switch from 32-bit protected mode with flat physical addresses to
 * 16-bit real mode.  Stack and code must be in base memory when
 * called.  %cs, %ss, %eip, %esp are changed to real-mode values,
 * other segment registers are destroyed, all other registers are
 * preserved.  Interrupts are *not* enabled.
 *
 * Parameters:
 *   %cs		Real-mode code segment (word)
 *   %ss		Real-mode stack segment (word)
 ****************************************************************************
 */

#define P2R_PRESERVE ( 12 )
#define P2R_OFFSET_RETADDR ( P2R_PRESERVE )
#define P2R_OFFSET_RETADDR_E ( P2R_OFFSET_RETADDR + 4 )
#define P2R_OFFSET_CS ( P2R_OFFSET_RETADDR_E )
#define P2R_OFFSET_CS_E ( P2R_OFFSET_CS + 2 )
#define P2R_OFFSET_SS ( P2R_OFFSET_CS_E )
#define P2R_OFFSET_SS_E ( P2R_OFFSET_SS + 2 )

	.globl _prot_to_real
	.code32
_prot_to_real:
	/* Preserve registers */
	pushl	%ebp
	pushl	%ebx
	pushl	%eax
	
	/* Calculate offset */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp		/* %ebp = offset for labels in p2r*/

	/* Set up GDT with real-mode limits and appropriate bases for
	 * real-mode %cs and %ss.  Set up protected-mode continuation
	 * point on stack.
	 */
	/* Fixup GDT */
	leal	p2r_gdt(%ebp), %eax
	movl	%eax, p2r_gdt_addr(%ebp)

	/* Calculate CS base address: set GDT code segment, adjust
	 * return address, set up continuation address on stack.
	 */
	movzwl	P2R_OFFSET_CS(%esp), %eax
	shll	$4, %eax
	/* Change return address to real-mode far address */
	subl	%eax, P2R_OFFSET_RETADDR(%esp)
	movl	%eax, %ebx
	shrl	$4, %ebx
	movw	%bx, (P2R_OFFSET_RETADDR+2)(%esp)
	/* Continuation address */
	pushl	$(p2r_rmcs - p2r_gdt)
	leal	2f(%ebp), %ebx
	subl	%eax, %ebx
	pushl	%ebx
	/* Code segment in GDT */
	movw	%ax, (p2r_rmcs+2)(%ebp)
	shrl	$16, %eax			/* Remainder of cs base addr */
	movb	%al, (p2r_rmcs+4)(%ebp)

	/* Calculate SS base address: set GDT data segment, retain to
	 * use for adjusting %esp.
	 */
	movzwl	(8+P2R_OFFSET_SS)(%esp), %eax	/* Set ss base address */
	shll	$4, %eax
	movw	%ax, (p2r_rmds+2)(%ebp)
	movl	%eax, %ebx
	shrl	$16, %ebx
	movb	%bl, (p2r_rmds+4)(%ebp)

	/* Load GDT */
	lgdt	p2r_gdt(%ebp)
	/* Reload all segment registers and adjust %esp */
	movw	$(p2r_rmds - p2r_gdt), %bx /* Pmode DS */
	movw	%bx, %ss
	subl	%eax, %esp		/* %esp now less than 0x10000 */
	movw	%bx, %ds
	movw	%bx, %es
	movw	%bx, %fs
	movw	%bx, %gs
	lret				/* %cs:eip */
2:	/* Segment registers now have 16-bit limits. */
	.code16

	/* Switch to real mode */
	movl	%cr0, %ebx
	andb	$0!CR0_PE, %bl
	movl	%ebx, %cr0
	
	/* Load real-mode segment value to %ss.  %sp already OK */
	shrl	$4, %eax
	movw	%ax, %ss

	/* Restore registers */
	popl	%eax
	popl	%ebx
	popl	%ebp

	/* Return to caller in real-mode */
	lret
	
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

	/* This is the end of the trampoline prefix code.  When used
	 * as a prefix, fall through to the following code in the
	 * trampoline.
	 */
p2r_params: /* Structure must match prot_to_real_params_t in realmode.h */
p2r_esp:	.long 0
p2r_segments:
p2r_cs:		.word 0
p2r_ss:		.word 0
p2r_r2p_params:	.long 0
	.globl	_prot_to_real_prefix_end
_prot_to_real_prefix_end:
	
	.globl _prot_to_real_prefix_size
_prot_to_real_prefix_size:	
	.word	 _prot_to_real_prefix_end - _prot_to_real_prefix
	
/****************************************************************************
 * _real_to_prot_suffix
 *
 * Trampoline fragment.  Switch from 16-bit real-mode to 32-bit
 * protected mode with flat physical addresses.  Copy returned stack
 * parameters to output_stack.  Restore registers preserved by
 * _prot_to_real_prefix.  Restore stack to previous location.
 ****************************************************************************
 */

	.globl _real_to_prot_suffix
	.code16		
_real_to_prot_suffix:

	/* Switch to protected mode */
	call	_real_to_prot
	.code32

	/* Calculate offset */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp		/* %ebp = offset for labels in r2p */

	/* Copy stack to out_stack */
	movl	r2p_out_stack(%ebp), %edi
	movl	r2p_out_stack_len(%ebp), %ecx
	movl	%esp, %esi
	cld
	rep movsb

	/* Switch back to original stack */
	movl	r2p_esp(%ebp), %esp

	/* Restore registers and return */
	pushl	r2p_ret_addr(%ebp)	/* Set up return address on stack */
	movl	r2p_ebx(%ebp), %ebx
	movl	r2p_esi(%ebp), %esi
	movl	r2p_edi(%ebp), %edi
	movl	r2p_ebp(%ebp), %ebp
	ret

/****************************************************************************
 * _real_to_prot
 *
 * Switch from 16-bit real-mode to 32-bit protected mode with flat
 * physical addresses.  All segment registers are destroyed, %eip and
 * %esp are changed to flat physical values, all other registers are
 * preserved.  Interrupts are disabled.
 *
 * Parameters: none
 ****************************************************************************
 */

#define R2P_PRESERVE ( 12 )
#define R2P_OFFSET_RETADDR ( R2P_PRESERVE )
#define R2P_OFFSET_RETADDR_E ( R2P_OFFSET_RETADDR + 8 )
#define R2P_OFFSET_ORIG_RETADDR_E ( R2P_OFFSET_RETADDR_E )
#define R2P_OFFSET_ORIG_RETADDR ( R2P_OFFSET_RETADDR_E - 2 )
#define R2P_INSERT_LENGTH ( R2P_OFFSET_ORIG_RETADDR - R2P_OFFSET_RETADDR )
	
	.globl _real_to_prot
	.code16		
_real_to_prot:
	/* Allow space for 32-bit far return address */
	subw	$R2P_INSERT_LENGTH, %sp
	
	/* Preserve registers */
	pushl	%ebp
	pushl	%ebx
	pushl	%eax

	/* Convert 16-bit real-mode near return address to
	 * 32-bit pmode physical far return address
	 */
	movw	%sp, %bp
	xorl	%eax, %eax
	push	%cs
	popw	%ax
	shll	$4, %eax
	movzwl	R2P_OFFSET_ORIG_RETADDR(%bp), %ebx
	addl	%eax, %ebx
	movl	%ebx, (R2P_OFFSET_RETADDR+0)(%bp)
	movl	$(r2p_pmcs-r2p_gdt), %ebx
	movl	%ebx, (R2P_OFFSET_RETADDR+4)(%bp)

	/* Set up GDT */
	call	1f
1:	popw	%bp
	leal	(r2p_gdt-1b)(%bp), %ebx	/* %cs:ebx = %cs:bx = &(r2p_gdt) */
	addl	%eax, %ebx		/* %eax = &r2p_gdt (physical) */
	movl	%ebx, %cs:(r2p_gdt-1b+2)(%bp) /* Set phys. addr. in r2p_gdt */

	/* Calculate new %esp */
	xorl	%eax, %eax
	push	%ss
	popw	%ax
	shll	$4, %eax
	movzwl	%sp, %ebx
	addl	%ebx, %eax		/* %eax = new %esp */
	
	/* Load GDT */
	cli				/* Disable interrupts */
	lgdt	%cs:(r2p_gdt-1b)(%bp)	/* Load GDT */

	/* Switch to protected mode */
	movl	%cr0, %ebx
	orb	$CR0_PE, %bl
	movl	%ebx, %cr0

	/* Load segment registers, adjust %esp */
	movw	$(r2p_pmds-r2p_gdt), %bx
	movw	%bx, %ss
	movl	%eax, %esp
	movw	%bx, %ds
	movw	%bx, %es
	movw	%bx, %fs
	movw	%bx, %gs

	/* Restore registers */
	popl	%eax
	popl	%ebx
	popl	%ebp

	/* Switch to flat physical addresses and return to caller */
	DATA32 lret

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

	/* This is the end of the trampoline suffix code.
	 */
r2p_params: /* Structure must match real_to_prot_params_t in realmode.h */
r2p_ret_addr:		.long 0
r2p_esp:		.long 0
r2p_ebx:		.long 0
r2p_esi:		.long 0
r2p_edi:		.long 0
r2p_ebp:		.long 0
r2p_out_stack:		.long 0
r2p_out_stack_len:	.long 0
	.globl	_real_to_prot_suffix_end
_real_to_prot_suffix_end:

	.globl _real_to_prot_suffix_size
_real_to_prot_suffix_size:
	.word	_real_to_prot_suffix_end - _real_to_prot_suffix

	
		
rm_callback_interface_end:

	.globl	rm_callback_interface_size
rm_callback_interface_size:
	.word	rm_callback_interface_end - rm_callback_interface

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

/* Macro to calculate offset of labels within code segment in
 * installed copy of code.
 */
#define INSTALLED(x) ( (x) - pxe_callback_interface )

/****************************************************************************
 * PXE entry points (!PXE and PXENV+ APIs)
 ****************************************************************************
 */
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

/****************************************************************************
 * Memory map mangling code
 ****************************************************************************
 */

/****************************************************************************
 * Exclude an address range from a potentially overlapping address range
 *
 * Note: this *can* be called even if the range doesn't overlap; it
 * will simply return the range unaltered.  It copes with all the
 * possible cases of overlap, including total overlap (which will
 * modify the range to length zero).  If the to-be-excluded range is
 * in the middle of the target range, then the larger remaining
 * portion will be returned.  If %di is nonzero on entry then the
 * range will only be truncated from the high end, i.e. the base
 * address will never be altered.  All this in less than 30
 * instructions.  :)
 *
 * Parameters:
 *  %eax	Base address of memory range
 *  %ecx	Length of memory range
 *  %ebx	Base address of memory range to exclude
 *  %edx	Length of memory range to exclude
 *  %di		0 => truncate either end, 1 => truncate high end only
 * Returns:
 *  %eax	Updated base address of range
 *  %ecx	Updated length of range
 *  %ebx,%edx	Undefined
 *		All other registers (including %di) preserved
 *
 * Note: "ja" is used rather than "jg" because we are comparing
 * unsigned ints
 ****************************************************************************
 */
#ifdef TEST_EXCLUDE_ALGORITHM
	.code32
#endif /* TEST_EXCLUDE_ALGORITHM */
exclude_memory_range:
	/* Convert (start,length) to (start,end) */
	addl	%eax, %ecx
	addl	%ebx, %edx
	/* Calculate "prefix" length */
	subl	%eax, %ebx		/* %ebx = "prefix" length */
	ja	1f
	xorl	%ebx, %ebx		/* Truncate to zero if negative */
1:	/* %di == 0 => truncate either end
	 * %di != 0 => truncate only high end
	 */
	testw	%di, %di
	je	use_either
	cmpl	%eax, %edx
	jbe	99f			/* excl. range is below target range */
use_prefix:	/* Use prefix, discard suffix */
	addl	%eax, %ebx		/* %ebx = candidate end address */
	cmpl	%ecx, %ebx		/* %ecx = min ( %ebx, %ecx ) */
	ja	1f
	movl	%ebx, %ecx
1:	jmp	99f
use_either:		
	/* Calculate "suffix" length */
	subl	%ecx, %edx		/* %edx = -( "suffix" length ) */
	jb	1f
	xorl	%edx, %edx		/* Truncate to zero if negative */
1:	negl	%edx			/* %edx = "suffix" length */
	/* Use whichever is longest of "prefix" and "suffix" */
	cmpl	%ebx, %edx
	jbe	use_prefix
use_suffix:	/* Use suffix, discard prefix */
	negl	%edx
	addl	%ecx, %edx		/* %edx = candidate start address */
	cmpl	%eax, %edx		/* %eax = max ( %eax, %edx ) */
	jb	1f
	movl	%edx, %eax
1:	
99:	subl	%eax, %ecx		/* Convert back to (start,length) */
	ret

#ifdef TEST_EXCLUDE_ALGORITHM
	.globl	__test_exclude
__test_exclude:
	pushl   %ebx
	pushl	%edi
	movl    12(%esp), %eax
	movl    16(%esp), %ecx
	movl    20(%esp), %ebx
	movl    24(%esp), %edx
	movl	28(%esp), %edi
	call    exclude_memory_range
	shll    $16, %eax
	orl     %ecx, %eax
	popl	%edi
	popl	%ebx
	ret
	.code16
#endif /* TEST_EXCLUDE_ALGORITHM */
	
/****************************************************************************
 * Exclude Etherboot-reserved address ranges from a potentially
 * overlapping address range
 *
 * Parameters:
 *  %eax	Base address of memory range
 *  %ecx	Length of memory range
 *  %di		0 => truncate either end, 1 => truncate high end only
 * Returns:
 *  %eax	Updated base address of range
 *  %ecx	Updated length of range
 *		All other registers (including %di) preserved
 ****************************************************************************
 */
exclude_hidden_memory_ranges:
	pushw	%si
	pushl	%ebx
	pushl	%edx
	movw	$INSTALLED(_pxe_hide_memory), %si
2:	movl	%cs:0(%si), %ebx
	movl	%cs:4(%si), %edx
	call	exclude_memory_range
	addw	$8, %si
	cmpw	$INSTALLED(_pxe_hide_memory_end), %si
	jl	2b
	popl	%edx
	popl	%ebx
	popw	%si
	ret
	
	.globl	_pxe_hide_memory	
_pxe_hide_memory:
	.long	0,0			/* Etherboot text (base,length) */
	.long	0,0			/* Heap (base,length) */
_pxe_hide_memory_end:

/****************************************************************************
 * Intercept INT 15 memory calls and remove the hidden memory ranges
 * from the resulting memory map.
 ****************************************************************************
 */
	.globl	_pxe_intercept_int15
_pxe_intercept_int15:
	/* Preserve registers */
	pushw	%bp
	/* Store %ax for future reference */
	pushw	%ax
	/* Make INT-style call to old INT15 routine */
	pushfw
	lcall	%cs:*INSTALLED(_pxe_intercepted_int15)
	/* Preserve flags returned by original E820 routine */
	pushfw
	/* Check for valid INT15 routine */
	jc	pxe_intercept_int15_exit
	/* Check for a routine we want to intercept */
	movw	%sp, %bp
	cmpw	$0xe820, 2(%bp)
	je	pxe_intercept_e820
	cmpw	$0xe801, 2(%bp)
	je	pxe_intercept_e801
	cmpb	$0x88, 3(%bp)
	je	pxe_intercept_88
pxe_intercept_int15_exit:
	/* Restore registers and return */
	popfw
	popw	%bp			/* discard original %ax */
	popw	%bp
	lret	$2			/* 'iret' - flags already loaded */

	.globl	_pxe_intercepted_int15
_pxe_intercepted_int15:	.word 0,0
		
/****************************************************************************
 * Intercept INT 15,E820 calls and remove the hidden memory ranges
 * from the resulting memory map.
 ****************************************************************************
 */
#define SMAP ( 0x534d4150 )
pxe_intercept_e820:
	/* Check for valid E820 routine */
	cmpl	$SMAP, %eax
	jne	pxe_intercept_int15_exit
	/* If base address isn't in the low 4GB, return unaltered
	 * (since we never claim memory above 4GB).  WARNING: we cheat
	 * by assuming that no E820 region will straddle the 4GB
	 * boundary: if this is not a valid assumption then things
	 * will probably break.
	 */
	cmpl	$0, %es:4(%di)
	jne	pxe_intercept_int15_exit
	/* Preserve registers */
	pushl	%eax
	pushl	%ecx
	/* Update returned memory range */
	movl	%es:0(%di), %eax	/* Base */
	movl	%es:8(%di), %ecx	/* Length */
	pushw	%di
	xorw	%di, %di		/* "truncate either end" flag */
	call	exclude_hidden_memory_ranges
	popw	%di
	movl	%eax, %es:0(%di)	/* Store updated base */
	movl	%ecx, %es:8(%di)	/* Store updated length */
	/* Restore registers and return */
	popl	%ecx
	popl	%eax
	jmp	pxe_intercept_int15_exit

/****************************************************************************
 * Intercept INT 15,E801 calls and remove the hidden memory ranges
 * from the resulting memory map.
 ****************************************************************************
 */
pxe_intercept_e801:
	/* Adjust return values */
	call	e801_adjust
	xchgw	%ax, %cx
	xchgw	%bx, %dx
	call	e801_adjust
	xchgw	%ax, %cx
	xchgw	%bx, %dx
	jmp	pxe_intercept_int15_exit
	
	/* %ax = #KB from 1MB+, %bx = #64KB from 16MB+
	 * Return with modified values in %ax, %bx.  Preserver other regs.
	 */
e801_adjust:
	pushw	%di
	pushl	%ecx
	pushl	%eax
	movw	$1, %di			/* "truncate only high end" flag */

	/* Truncate #64KB from 16MB+ as appropriate */
	movw	%bx, %cx		/* (no need to zero high word) */
	shll	$16, %ecx		/* %ecx = length in bytes */
	movl	$(1<<24), %eax		/* 16MB start address */
	call	exclude_hidden_memory_ranges
	shrl	$16, %ecx		/* %cx = updated length in 64KB */
	movw	%cx, %bx		/* Return in %bx */
	
	/* Truncate #KB from 1MB+ as appropriate */
	popw	%cx			/* Orig. %ax (high word already 0) */
	shll	$10, %ecx		/* %ecx = length in bytes */
	shrl	$4, %eax		/* 1MB start address */
	call	exclude_hidden_memory_ranges
	shrl	$10, %ecx		/* %cx = updated length in KB */
	pushw	%cx			/* Will be picked up in %eax */
	
	popl	%eax
	popl	%ecx
	popw	%di
	ret

/****************************************************************************
 * Intercept INT 15,88 calls and remove the hidden memory ranges
 * from the resulting memory map.
 ****************************************************************************
 */
pxe_intercept_88:
	pushw	%bx			/* E801 adjust, ignore %bx */
	call	e801_adjust
	popw	%bx
	jmp	pxe_intercept_int15_exit
		
pxe_rm_in_call:	
pxe_attach_rm:
	/* rm_callback_interface must be appended here */

pxe_callback_interface_end:

	.globl	pxe_callback_interface_size
pxe_callback_interface_size:
	.word	pxe_callback_interface_end - pxe_callback_interface
	
#endif /* PXE_EXPORT */
	
#endif /* CODE16 */
