        .text
        .code16
	.globl _start

/* Cheat a little with the relocations: .COM files are loaded at 0x100 */
_start: 
        push    %cs                     # generate return address
        movw    $retaddr - _start +0x100, %ax
        pushw   %ax

/* Intel didn't specify an indirect far call that loads the address from
 * a near operand - it seems like nobody does computed far calls.  So do it
 * the ugly way - dump it on the stack and "return" to the destination.
 */
        movw    %cs,%ax                 /* calculate start address of loader */
        movw    $_body - _start +0x100, %bx
        movb    $4, %cl
        shrw    %cl,%bx
        addw    %bx,%ax
        pushw   %ax                     /* new code segment */
	xorw	%ax, %ax
        pushw   %ax                     /* new offset */

        lret
retaddr: 
	movw	$0x4c00,%ax
        int     $0x21                   /* should never reach this */


/* The body of etherboot is attached here at build time.
 * Force 16 byte alignment (so that the code segment starts at 0)
 */
        .align 16,0
_body: 

