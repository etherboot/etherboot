/* Null prefix:	simply calls Etherboot and returns afterwards */
	
	.org	0
	.text
	.section ".prefix", "ax", @progbits
	.code16

	.globl	_prefix
_prefix:
	/* Assume invoked via a near call */
	popw	%ax		/* Return offset */
	pushw	%cs		/* Convert to far return address */
	pushw	%ax
	/* Ensure %cs:0000 points to start of image */
	call	1f
1:	popw	%bx
	subw	$(1b-_prefix), %bx
	pushw	%cs
	popw	%ax		/* %ax:%bx points to _prefix */
	shrw	$4, %bx
	addw	%bx, %ax	/* %ax:0000 points to _prefix */
	
	pushl	$4		/* Preserve far return address */
	pushw	$0		/* Use prefix exit mechanism */

	/* Calculated lcall to _start with %cs:0000 = _prefix */
	pushw	%ax		
	pushw	$_start
	lret
	
	.section ".text16", "ax", @progbits
	.globl	prefix_exit
prefix_exit:
	lret			/* Return to caller */
	.globl	prefix_exit_end
prefix_exit_end:
	.previous
