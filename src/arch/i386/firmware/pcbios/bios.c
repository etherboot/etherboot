/* Etherboot routines for PCBIOS firmware.
 *
 * Body of routines taken from old pcbios.S
 */

#ifdef PCBIOS

#include "etherboot.h"
#include "realmode.h"
#include "segoff.h"

#define ZF ( 1 << 6 )
#define CF ( 1 << 0 )

/**************************************************************************
CURRTICKS - Get Time
Use direct memory access to BIOS variables, longword 0040:006C (ticks
today) and byte 0040:0070 (midnight crossover flag) instead of calling
timeofday BIOS interrupt.
**************************************************************************/
#if defined(CONFIG_TSC_CURRTICKS)
#undef CONFIG_BIOS_CURRTICKS
#else
#define CONFIG_BIOS_CURRTICKS 1
#endif
#if defined(CONFIG_BIOS_CURRTICKS)
unsigned long currticks (void) {
	static uint32_t days = 0;
	uint32_t *ticks = VIRTUAL(0x0040,0x006c);
	uint8_t *midnight = VIRTUAL(0x0040,0x0070);

	/* Re-enable interrupts so that the timer interrupt can occur
	 */
	BEGIN_RM_FRAGMENT(rm_currticks);
	__asm__ ( "sti" );
	__asm__ ( "nop" );
	__asm__ ( "nop" );
	__asm__ ( "cli" );
	END_RM_FRAGMENT(rm_currticks);

	real_call ( rm_currticks, NULL, NULL );

	if ( *midnight ) {
		*midnight = 0;
		days += 0x1800b0;
	}
	return ( days + *ticks );
}
#endif	/* CONFIG_BIOS_CURRTICKS */

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

/**************************************************************************
INT15 - Call Interrupt 0x15
**************************************************************************/
int int15 ( int ax ) {
	struct {
		reg16_t ax;
	} PACKED in_stack;
	struct {
		reg16_t flags;
	} PACKED out_stack;
	reg16_t ret_ax;

	BEGIN_RM_FRAGMENT(rm_int15);
	__asm__ ( "sti" );
	__asm__ ( "popw %ax" );
	__asm__ ( "stc" );
	__asm__ ( "int $0x15" );
	__asm__ ( "pushf" );
	__asm__ ( "cli" );
	END_RM_FRAGMENT(rm_int15);

	in_stack.ax.word = ax;
	ret_ax.word = real_call ( rm_int15, &in_stack, &out_stack );

	/* Carry flag clear indicates function not supported */
	if ( ! ( out_stack.flags.word & CF ) ) return 0;
	return ret_ax.h;
}

#ifdef	POWERSAVE
/**************************************************************************
CPU_NAP - Save power by halting the CPU until the next interrupt
**************************************************************************/
void cpu_nap ( void ) {
	BEGIN_RM_FRAGMENT(rm_cpu_nap);
	__asm__ ( "sti" );
	__asm__ ( "hlt" );
	__asm__ ( "cli" );
	END_RM_FRAGMENT(rm_cpu_nap);

	real_call ( rm_cpu_nap, NULL, NULL );
}
#endif	/* POWERSAVE */

#endif /* PCBIOS */
