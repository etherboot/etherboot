/* A couple of routines to implement a low-overhead timer for drivers */

 /*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include	"etherboot.h"
#include	"timer.h"

void load_timer2(unsigned int ticks)
{
	/* Set up the timer gate, turn off the speaker */
	outb((inb(PPC_PORTB) & ~PPCB_SPKR) | PPCB_T2GATE, PPC_PORTB);
	outb(TIMER2_SEL|WORD_ACCESS|MODE0|BINARY_COUNT, TIMER_MODE_PORT);
	outb(ticks & 0xFF, TIMER2_PORT);
	outb(ticks >> 8, TIMER2_PORT);
}
