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
	/*
	 * Now let's take care of PPC channel 2
	 *
	 * Set the Gate high, program PPC channel 2 for mode 0,
	 * (interrupt on terminal count mode), binary count,
	 * load 5 * LATCH count, (LSB and MSB) to begin countdown.
	 *
	 * Note some implementations have a bug where the high bits byte
	 * of channel 2 is ignored.
	 */
	/* Set up the timer gate, turn off the speaker */
	/* Set the Gate high, disable speaker */
	outb((inb(PPC_PORTB) & ~PPCB_SPKR) | PPCB_T2GATE, PPC_PORTB);
	/* binary, mode 0, LSB/MSB, Ch 2 */
	outb(TIMER2_SEL|WORD_ACCESS|MODE0|BINARY_COUNT, TIMER_MODE_PORT);
	/* LSB of ticks */
	outb(ticks & 0xFF, TIMER2_PORT);
	/* MSB of ticks */
	outb(ticks >> 8, TIMER2_PORT);
}

#if defined(CONFIG_TSC_CURRTICKS)
#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define rdtscll(val) \
     __asm__ __volatile__ ("rdtsc" : "=A" (val))


/* Number of clock ticks to time with the rtc */
#define LATCH 0xFF

#define LATCHES_PER_SEC ((CLOCK_TICK_RATE + (LATCH/2))/LATCH)
#define TICKS_PER_LATCH ((LATCHES_PER_SEC + (TICKS_PER_SEC/2))/TICKS_PER_SEC)

/* ------ Calibrate the TSC ------- 
 * Time how long it takes to excute a loop that runs in known time.
 * And find the convertion needed to get to CLOCK_TICK_RATE
 */


static unsigned long long calibrate_tsc(void)
{
	unsigned long startlow, starthigh;
	unsigned long endlow, endhigh;
	unsigned long count;
	
	load_timer2(LATCH);
	rdtsc(startlow,starthigh);
	count = 0;
	do {
		count++;
	} while (timer2_running());
	rdtsc(endlow,endhigh);
	
	/* Error: ECTCNEVERSET */
	if (count <= 1)
		goto bad_ctc;
	
	/* 64-bit subtract - gcc just messes up with long longs */
	__asm__("subl %2,%0\n\t"
		"sbbl %3,%1"
		:"=a" (endlow), "=d" (endhigh)
		:"g" (startlow), "g" (starthigh),
		"0" (endlow), "1" (endhigh));
	
	/* Error: ECPUTOOFAST */
	if (endhigh)
		goto bad_ctc;
	
	endlow *= TICKS_PER_LATCH;
	return endlow;

	/*
	 * The CTC wasn't reliable: we got a hit on the very first read,
	 * or the CPU was so fast/slow that the quotient wouldn't fit in
	 * 32 bits..
	 */
bad_ctc:
	printf("bad_ctc\n");
	return 0;
}


unsigned long currticks(void)
{
	static unsigned long clocks_per_tick;
	unsigned long clocks_high, clocks_low;
	unsigned long currticks;
	if (!clocks_per_tick) {
		clocks_per_tick = calibrate_tsc();
		/* Display the CPU Mhz to easily test if the calibration was bad */
		printf("CPU %d Mhz\n", (clocks_per_tick/1000 * TICKS_PER_SEC)/1000);
	}

	/* Read the Time Stamp Counter */
	rdtsc(clocks_low, clocks_high);

	/* currticks = clocks / clocks_per_tick; */
	__asm__("divl %1"
		:"=a" (currticks)
		:"r" (clocks_per_tick), "0" (clocks_low), "d" (clocks_high));


	return currticks;
}

#endif /* RTC_CURRTICKS */
