/* Etherboot routines for PCBIOS firmware.
 *
 * Body of routines taken from old pcbios.S
 */

#ifdef PCBIOS

#include "etherboot.h"
#include "realmode.h"
#include "segoff.h"

#define ZF ( 1 << 6 )

/**************************************************************************
CONSOLE_PUTC - Print a character on console
**************************************************************************/
void console_putc ( int character ) {
	struct {
		reg16_t ax;
	} PACKED in_stack;

	BEGIN_RM_FRAGMENT(rm_console_putc);
	__asm__ ( "sti" );
	__asm__ ( "popw %ax" );
	__asm__ ( "movb $0x0e, %ah" );
	__asm__ ( "movl $1, %ebx" );
	__asm__ ( "int $0x10" );
	__asm__ ( "cli" );
	END_RM_FRAGMENT(rm_console_putc);

	in_stack.ax.l = character;
	real_call ( rm_console_putc, &in_stack, NULL );
}

/**************************************************************************
CONSOLE_GETC - Get a character from console
**************************************************************************/
int console_getc ( void ) {
	BEGIN_RM_FRAGMENT(rm_console_getc);
	__asm__ ( "sti" );
	__asm__ ( "xorw %ax, %ax" );
	__asm__ ( "int $0x16" );
	__asm__ ( "xorb %ah, %ah" );
	__asm__ ( "cli" );
	END_RM_FRAGMENT(rm_console_getc);

	return real_call ( rm_console_getc, NULL, NULL );
}

/**************************************************************************
CONSOLE_ISCHAR - Check for keyboard interrupt
**************************************************************************/
int console_ischar ( void ) {
	BEGIN_RM_FRAGMENT(rm_console_ischar);
	__asm__ ( "sti" );
	__asm__ ( "movb $1, %ah" );
	__asm__ ( "int $0x16" );
	__asm__ ( "pushfw" );
	__asm__ ( "popw %ax" );
	__asm__ ( "cli" );
	END_RM_FRAGMENT(rm_console_ischar);

	return ( ( real_call ( rm_console_ischar, NULL, NULL ) & ZF ) == 0 );
}

/**************************************************************************
GETSHIFT - Get keyboard shift state
**************************************************************************/
int getshift ( void ) {
	BEGIN_RM_FRAGMENT(rm_getshift);
	__asm__ ( "sti" );
	__asm__ ( "movb $2, %ah" );
	__asm__ ( "int $0x16" );
	__asm__ ( "andw $0x3, %ax" );
	__asm__ ( "cli" );
	END_RM_FRAGMENT(rm_getshift);

	return real_call ( rm_getshift, NULL, NULL );
}

#endif /* PCBIOS */
