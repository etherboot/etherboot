        .text
        .code16

/* Cheat a little with the relocations: .COM files are loaded at 0x100 */
_main: 
        push    %cs                     # generate return address
        movw    $retaddr+0x100, %ax
        pushw   %ax

/* Intel didn't specify an indirect far call that loads the address from
 * a near operand - it seems like nobody does computed far calls.  So do it
 * the ugly way - dump it on the stack and "return" to the destination.
 */
        movw    %cs,%ax                 /* calculate start address of loader */
        movw    $_body+0x100, %bx
        movb    $4, %cl
        shrw    %cl,%bx
        addw    %bx,%ax
        pushw   %ax                     /* new code segment */
        movw    $0x0006, %ax
        pushw   %ax                     /* new offset */

        lret
retaddr: 
        int     $0x19                   /* should never reach this */


/* The body of etherboot is attached here at build time.
 * Force 16 byte alignment (so that the code segment starts at 0)
 */
        .align 16,0
_body: 

