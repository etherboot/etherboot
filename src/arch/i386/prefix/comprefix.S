#define EXEC_IN_SITU_MAGIC	0x45524548	/* 'HERE' */
	
/* We need a real mode stack that won't be stomped on by Etherboot
   which starts at 0x20000. Choose something that's sufficiently high,
   but not in DOC territory. Note that we couldn't do this in a real
   .com program since stack variables are in the same segment as the
   code and data, but this isn't really a .com program, it just looks
   like one to make DOS load it into memory. It still has the 64kB
   limitation of .com files though. */
#define STACK_SEG	0x7000

	.text
	.code16
	.globl _start

/* Cheat a little with the relocations: .COM files are loaded at 0x100 */
_start: 
	movw	$STACK_SEG,%ax
	movw	%ax,%ss
	push	%cs			# generate return address
	movw	$retaddr-_start +0x100,%ax
	pushw	%ax

/* Intel didn't specify an indirect far call that loads the address from
 * a near operand - it seems like nobody does computed far calls.  So do it
 * the ugly way - dump it on the stack and "return" to the destination.
 */
	movw	%cs,%ax			/* calculate start address of loader */
	movw	$_body - _start +0x100,%bx
	movb	$4,%cl
	shrw	%cl,%bx
	addw	%bx,%ax
	pushw	%ax			/* new code segment */
	xorw	%ax,%ax
	pushw	%ax			/* new offset */

/*
 * Load ES:DI with "HE:RE" - the magic signature that prevents
 * start16.S from relocating the payload to an area of memory claimed
 * from the BIOS.
 *
 * This is new behaviour for start16 (added by Michael Brown on
 * 2004/01/02).  I have no idea how .com images should interact with
 * DOS memory allocation.  By setting this magic signature we get
 * behaviour as before, so at least it won't make things worse.
 */
	push	$(EXEC_IN_SITU_MAGIC >> 16)
	pop	%es
	mov	$(EXEC_IN_SITU_MAGIC & 0xffff), %di
		
	lret				/* The actual jump to start16.S */
retaddr: 
	movw	$0x4c00,%ax		/* return to DOS */
	int	$0x21			/* reach this on Quit */

/* The body of etherboot is attached here at build time.
 * Force 16 byte alignment (so that the code segment starts at 0)
 */
	.align 16,0
_body: 
