	.text
	.code16
	.section	".prefix", "ax", @progbits
	.globl	_prefix
#	.globl	_verbatim_size

/* We need a real mode stack that won't be stomped on by Etherboot
   which starts at 0x20000. Choose something that's sufficiently high,
   but not in DOC territory. Note that we couldn't do this in a real
   .com program since stack variables are in the same segment as the
   code and data, but this isn't really a .com program, it just looks
   like one to make DOS load it into memory. It still has the 64kB
   limitation of .com files though. */
#define STACK_SEG	0x7000
#define STACK_SIZE	0x4000

_prefix:
	.byte	'M', 'Z'
	.short	_verbatim_size_tail		/* tail */
	.short	_verbatim_size_pages		/* pages */
	.short	0				/* relocations */
	.short	2				/* header paras */
	.short	0x1000				/* min */
	.short	0xFFFF				/* max paras */
	.short	0xFFF0				/* SS */
	.short	0xFFFE				/* SP */
	.short	0				/* checksum */
	.short	0x100				/* IP */
	.short	0xFFF0				/* CS */
	.short	0x1C				/* reloc offset */
	.short	0				/* overlay number */
	.short	0				/* fill */
	.short	0				/* fill */

	/* Set up temporary stack */ 
	movw	$STACK_SEG, %ax
	movw	%ax, %ss
	movw	$STACK_SIZE, %sp

	pushl	$0	/* No parameters to preserve for exit path */
	pushw	$0	/* Dummy return address - use prefix_exit */

	/* Calculate segment address of image start */
	pushw	%cs
	popw	%ax
	addw	$(0x100/16), %ax
	pushw	%ax
	pushw	$_start
	/* Calculated lcall to _start with %cs:0000 = image start */
	lret

	.section ".text16", "ax", @progbits
	.globl	prefix_exit
prefix_exit:
	movw	$0x4c00,%ax		/* return to DOS */
	int	$0x21			/* reach this on Quit */
	.globl	prefix_exit_end
prefix_exit_end:
	.previous

/* The body of etherboot is attached here at build time.
 * Force 16 byte alignment
 */
	.align 16,0
_body: 
