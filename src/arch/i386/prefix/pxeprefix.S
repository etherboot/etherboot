/* Offsets of words containing ROM's CS and size (in 512 byte blocks)
 * from start of floppy boot block at 0x7c00
 * Offsets must match those in etherboot.h
 */
#define FLOPPY_SEGMENT	0x7c0
#define ROM_SEGMENT	0x1fa
#define ROM_LENGTH	0x1fc

#define PXENV_UNDI_CLEANUP	0x02
#define PXENV_UNDI_SHUTDOWN	0x05
#define	PXENV_STOP_UNDI		0x15
#define PXENV_UNLOAD_STACK	0x70
#define PXENV_STOP_BASE		0x76

	.text
	.code16
	.org 0
	.globl _start
/*****************************************************************************
 * Entry point:	set cs, ds, bp, print welcome message
 *****************************************************************************
 */	
_start:
	jmp	$FLOPPY_SEGMENT, $code_start-_start
10:	.ascii	"PXE->EB "
20:
code_start:
	movw	%sp, %bp
	push	%cs
	pop	%ds
	movw	$10b-_start, %si
	movw	$20b-10b, %cx
	call	print_message

#ifndef PXELOADER_KEEP_ALL

/*****************************************************************************
 * Detect type of PXE available (!PXE, PXENV+ or none)
 *****************************************************************************
 */
detect_pxe:
	les	4(%bp), %di		/* !PXE structure */
	cmpl	$0x45585021, %es:(%di)	/* '!PXE' signature */
	je	detected_pxe
	mov	$0x5650, %ax
	int	$0x1a
	jc	detected_none
	cmp	$0x564e, %ax
	jne	detected_none
	cmpl	$0x4e455850, %es:(%bx)	/* 'PXEN' signature */
	jne	detected_none
	cmpw	$0x2b56, %es:4(%bx)	/* 'V+' signature */
	jne	detected_none
	cmpw	$0x201, %es:6(%bx)	/* API version 2.1 or higher */
	jb	detected_pxenv		/* otherwise use PXENV+ */
	les	%es:0x28(%bx), %di
	cmpl	$0x45585021, %es:(%di)	/* '!PXE' signature */
	je	detected_pxe
detected_none:	
	movw	$10f-_start, %si
	movw	$20f-10f, %cx
	call	print_message
	jmp	finished_with_error
10:	.ascii	"No PXE"
20:

10:	.ascii	"!PXE "
20:
detected_pxe:	/* es:di points to !PXE structure */
	push	%es
	push	%di
	push	%es:0x30(%di)		/* UNDI code segment */
	push	%es:0x36(%di)		/* UNDI code size */
	push	%es:0x28(%di)		/* UNDI data segment */
	push	%es:0x2e(%di)		/* UNDI data size */
	les	%es:0x10(%di), %di	/* Entry point to %es:%di */
	movw	$10b-_start, %si
	movw	$20b-10b, %cx
	jmp	pxe_setup_done

10:	.ascii	"PXENV+ "
20:
detected_pxenv: /* es:bx points to PXENV+ structure */
	push	%es
	push	%bx
	push	%es:0x24(%bx)		/* UNDI code segment */
	push	%es:0x26(%bx)		/* UNDI code size */
	push	%es:0x20(%bx)		/* UNDI data segment */
	push	%es:0x22(%bx)		/* UNDI data size */
	les	%es:0x0a(%bx), %di	/* Entry point to %es:%di */
	movw	$10b-_start, %si
	movw	$20b-10b, %cx

pxe_setup_done:
	mov	%es, pxe_entry_segment - _start
	mov	%di, pxe_entry_offset - _start
	pop	%ax
	mov	%ax, undi_data_size - _start
	pop	%ax
	mov	%ax, undi_data_segment - _start
	pop	%ax
	mov	%ax, undi_code_size - _start
	pop	%ax
	mov	%ax, undi_code_segment - _start
	call	print_message
	pop	%di
	pop	%es	/* Exit with %es:%di containing structure address */

/*****************************************************************************
 * Print information about located structure
 *****************************************************************************
 */
print_structure_information:
	call	print_segoff	/* %es:%di contains address of structure */
	les	%ds:(pxe_entry_segoff - _start), %di
	call	print_segoff
	les	%ds:(undi_code_segoff - _start), %di
	call	print_segoff
	les	%ds:(undi_data_segoff - _start), %di
	call	print_segoff
	
/*****************************************************************************
 * Unload PXE base code and UNDI driver
 *****************************************************************************
 */
unload_pxe:
	mov	$PXENV_UNLOAD_STACK, %bx
	call	pxe_call
	mov	$PXENV_STOP_UNDI, %bx
	call	pxe_call
	mov	$PXENV_UNDI_CLEANUP, %bx
	call	pxe_call
	/* On exit, zero flag is set iff all calls were successful */
		
/*****************************************************************************
 * Free base memory
 *****************************************************************************
 */
free_base_mem:
	mov	$0x40, %cx		/* Set up %fs for access to 40:13 */
	mov	%cx, %fs

	jnz	do_not_free_base_mem	/* Using zero flag from unload_pxe */

	mov	undi_code_segment - _start, %bx
	mov	undi_data_segment - _start, %cx
	mov	undi_code_size - _start, %ax
	cmp	%bx, %cx
	jb	1f
	mov	%cx, %bx
	mov	undi_data_size - _start, %ax
1:	add	$0x0f, %ax		/* Round up to next segment */
	shr	$4, %ax
	add	%bx, %ax		/* Highest segment address into %ax */
	add	$(1024 / 16 - 1), %ax	/* Round up to next kb */
	shr	$6, %ax			/* New free basemem size in %ax */
	mov	%fs:(0x13), %bx		/* Old free base memory in %bx */
	mov	%ax, %fs:(0x13)		/* Store new free base memory size */

	/* Note that zero_mem_loop will also zero out our stack, so make
	 * sure the stack is empty at this point.
	 */
	mov	%ax, %dx
	sub	%bx, %dx		/* numberof kb to zero in %dx */
	shl	$6, %bx			/* Segment address into %bx */
zero_mem_loop:
	mov	%bx, %es		/* kB boundary into %es:00 */
	xor	%ax, %ax
	xor	%di, %di
	mov	$0x400, %cx
	rep	stosb			/* fill kB with zeroes */
	add	$(1024 / 16), %bx
	dec	%dx
	jnz	zero_mem_loop
	/* Will exit here with zero flag set, so no need to set it explicitly
	 * in order to indicate success.
	 */
	
do_not_free_base_mem:
	pushf				/* Save success (zero) flag status */
	mov	%fs:(0x13), %ax		/* Free base memory in %ax */
	call	print_hex_word		/* Print free base memory */
	popf				/* Restore success (zero) flag */

#else  /* PXELOADER_KEEP_ALL */
	xor	%ax, %ax		/* Force zero flag to show success */
#endif /* PXELOADER_KEEP_ALL */

/*****************************************************************************
 * Exit point
 * Jump to finished with the zero flag set to indicate success, or to
 * finished_with_error to always report an error
 *****************************************************************************
 */	
finished:
	movw	$10f-_start, %si
	jz	1f
finished_with_error:
	movw	$11f-_start, %si
1:	movw	$20f-10f,%cx
	call	print_message
	ljmp	$FLOPPY_SEGMENT + (512/16), $0

10:	.ascii " ok \r\n"
20:	
11:	.ascii " err\r\n" /* Must be same length as "OK" string */
21:	

/*****************************************************************************
 * Subroutine: print character in %al (with LF -> LF,CR translation)
 *****************************************************************************
 */
print_character:
	movw	$0x0007, %bx		/* page 0, attribute 7 (normal) */
	movb	$0x0e, %ah		/* write char, tty mode */
	int	$0x10
	ret
	
/*****************************************************************************
 * Subroutine: print hex word in %ax
 *****************************************************************************
 */
print_hex_word:
	mov	$4, %cx
1:
	push	%ax
	shr	$12, %ax
	/* Courtesy of Norbert Juffa <norbert.juffa@amd.com> */
	cmp	$10, %al	
	sbb	$0x69, %al
	das
	call	print_character
	pop	%ax
	shl	$4, %ax
	loop	1b
	ret
	
/*****************************************************************************
 * Subroutine: print segment:offset address in %es:%di
 *****************************************************************************
 */
print_segoff:
	push	%di
	push	%es
	pop	%ax
	call	print_hex_word
	movb	$0x3a,%al			/* ':' */
	call	print_character
	pop	%ax
	call	print_hex_word
	mov	$0x20, %al			/* ' ' */
	call	print_character
	ret
	
/*****************************************************************************
 * Subroutine: print a message starting at %si for length %cx
 *****************************************************************************
 */	
print_message:
10: 	lodsb
	call	print_character
	loop	10b
	ret

/*****************************************************************************
 * Make a PXE API call.  Works with either !PXE or PXENV+ API.
 * Opcode in %bx.  pxe_parameter_structure always used.
 * Returns status code (not exit code) in %bx and prints it.
 * ORs status code with overall status code in pxe_overall_status, returns
 * with zero flag set iff all PXE API calls have been successful.
 *****************************************************************************
 */
pxe_call:
	/* Set up registers for PXENV+ API.  %bx already set up */
	push	%ds
	pop	%es
	mov	$pxe_parameter_structure - _start, %di
	/* Set up stack for !PXE API */
	pushw   %cs
	pushw	%di
	pushw	%bx
	/* Make the API call */
	lcall	*(pxe_entry_segoff - _start)
	/* Reset the stack */
	add	$6, %sp
	mov	pxe_parameter_structure - _start, %ax
	push	%ax
	call	print_hex_word
	mov	$0x20, %ax		/* ' ' */
	call	print_character
	pop	%bx
	or	%bx, pxe_overall_status - _start
	ret

/*****************************************************************************
 * PXE data structures
 *****************************************************************************
 */

pxe_overall_status:	.word 0

pxe_entry_segoff:
pxe_entry_offset:	.word 0
pxe_entry_segment:	.word 0

undi_code_segoff:
undi_code_size:		.word 0
undi_code_segment:	.word 0

undi_data_segoff:
undi_data_size:		.word 0
undi_data_segment:	.word 0

pxe_parameter_structure:
	.word	0
	.word	0,0,0,0,0

end_of_pxeloader:

	.org	ROM_SEGMENT
	.word	0
	.word	0
	.word	0xaa55
	.org 512
