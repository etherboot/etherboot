/* Etherboot routines for PCBIOS firmware.
 *
 * Body of routines taken from old pcbios.S
 */

#ifdef PCBIOS

#include "etherboot.h"
#include "realmode.h"
#include "segoff.h"

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

#ifdef	CAN_BOOT_DISK
/**************************************************************************
DISK_INIT - Initialize the disk system
**************************************************************************/
void disk_init ( void ) {
	BEGIN_RM_FRAGMENT(rm_disk_init);
	__asm__ ( "xorw %ax,%ax" );
	__asm__ ( "movb $0x80,%dl" );
	__asm__ ( "int $0x13" );
	END_RM_FRAGMENT(rm_disk_init);
	
	real_call ( rm_disk_init, NULL, NULL );
}

/**************************************************************************
DISK_READ - Read a sector from disk
**************************************************************************/
unsigned int pcbios_disk_read ( int drive, int cylinder, int head, int sector,
				char *buf ) {
	struct {
		reg16_t ax;
		reg16_t cx;
		reg16_t dx;
		segoff16_t buffer;
	} PACKED in_stack;
	struct {
		reg16_t flags;
	} PACKED out_stack;
	reg16_t ret_ax;

	BEGIN_RM_FRAGMENT(rm_pcbios_disk_read);
	__asm__ ( "popw %ax" );
	__asm__ ( "popw %cx" );
	__asm__ ( "popw %dx" );
	__asm__ ( "popw %bx" );
	__asm__ ( "popw %es" );
	__asm__ ( "int $0x13" );
	__asm__ ( "pushfw" );
	END_RM_FRAGMENT(rm_pcbios_disk_read);

	in_stack.ax.h = 2; /* INT 13,2 - Read disk sector */
	in_stack.ax.l = 1; /* Read one sector */
	in_stack.cx.h = cylinder & 0xff;
	in_stack.cx.l = ( ( cylinder >> 8 ) & 0x3 ) | sector;
	in_stack.dx.h = head;
	in_stack.dx.l = drive;
	in_stack.buffer.segment = SEGMENT ( buf );
	in_stack.buffer.offset = OFFSET ( buf );
	ret_ax = real_call ( rm_pcbios_disk_read, &in_stack, &out_stack );
	return ( out_stack.flags.word & CF ) ? ret_ax.word : 0;
}
#endif /* CAN_BOOT_DISK */

#endif /* PCBIOS */
