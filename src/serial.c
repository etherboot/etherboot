#include "etherboot.h"
#if defined(CONSOLE_SERIAL) || defined(CONSOLE_DUAL)

/*
 * The serial port interface routines implement a simple polled i/o
 * interface to a standard serial port.  Due to the space restrictions
 * for the boot blocks, no BIOS support is used (since BIOS requires
 * expensive real/protected mode switches), instead the rudimentary
 * BIOS support is duplicated here.
 *
 * The base address and speed for the i/o port are passed from the
 * Makefile in the COMCONSOLE and CONSPEED preprocessor macros.  The
 * line control parameters are currently hard-coded to 8 bits, no
 * parity, 1 stop bit (8N1).  This can be changed in init_serial().
 */

static int found = 0;

#define UART_BASE COMCONSOLE

#ifndef CONSPEED
#define CONSPEED 115200
#endif

#if ((115200%CONSPEED) != 0)
#error Bad ttys0 baud rate
#endif

#define COMBRD (115200/CONSPEED)

/* Line Control Settings */
#ifndef	COMPARM
/* Set 8bit, 1 stop bit, no parity */
#define	COMPARM	0x03
#endif

#define UART_LCS COMPARM

/* Data */
#define UART_RBR 0x00
#define UART_TBR 0x00

/* Control */
#define UART_IER 0x01
#define UART_IIR 0x02
#define UART_FCR 0x02
#define UART_LCR 0x03
#define UART_MCR 0x04
#define UART_DLL 0x00
#define UART_DLM 0x01

/* Status */
#define UART_LSR 0x05
#define UART_MSR 0x06
#define UART_SCR 0x07

/*
 * void serial_putc(int ch);
 *	Write character `ch' to port COMCONSOLE.
 */
void serial_putc(int ch)
{
	int i;
	int status;
	if (!found) {
		/* no serial interface */
		return;
	}
	i = 10000; /* timeout */
	while(--i > 0) {
		status = inb(COMCONSOLE + UART_LSR);
		if (status & (1 << 5)) { 
			/* TX buffer emtpy */
			outb(ch, COMCONSOLE + UART_TBR);
			break;
		}
	}
}

/*
 * int serial_getc(void);
 *	Read a character from port COMCONSOLE.
 */
int serial_getc(void)
{
	int status;
	int ch;
	do {
		status = inb(COMCONSOLE + UART_LSR);
	} while((status & 1) == 0);
	ch = inb(COMCONSOLE + UART_RBR);	/* fetch (first) character */
	ch &= 0x7f;				/* remove any parity bits we get */
	if (ch == 0x7f) {			/* Make DEL... look like BS */
		ch = 0x08;
	}
	return ch;
}

/*
 * int serial_ischar(void);
 *       If there is a character in the input buffer of port COMCONSOLE,
 *       return nonzero; otherwise return 0.
 */
int serial_ischar(void)
{
	int status;
	if (!found)
		return 0;
	status = inb(COMCONSOLE + UART_LSR);	/* line status reg; */
	return status & 1;		/* rx char available */
}

#if	!defined(COMBRD) && defined(CONSPEED)
/* Recent GNU as versions with ELF output format define / as a comment
 * character, because some misguided spec says so.  Do it the easy way and
 * just check for the usual values.  This is only compiled by gcc, so
 * #elif can be used (bcc doesn't understand it).  */
#if	(CONSPEED == 115200)
#define COMBRD 1
#elif	(CONSPEED == 57600)
#define COMBRD 2
#elif	(CONSPEED == 38400)
#define COMBRD 3
#elif	(CONSPEED == 19200)
#define COMBRD 6
#elif	(CONSPEED == 9600)
#define COMBRD 12
#elif	(CONSPEED == 2400)
#define COMBRD 48
#elif	(CONSPEED == 1200)
#define COMBRD 96
#elif	(CONSPEED == 300)
#define COMBRD 384
#else
#error Add your unusual baud rate to the table in serial.S!
#define	COMBRD	(115200 / CONSPEED)
#endif
#endif


/*
 * int serial_init(void);
 *	Initialize port COMCONSOLE to speed CONSPEED, line settings 8N1.
 */
int serial_init(void)
{
	int initialized = 0;
	int status;
	int divisor, lcs;

	divisor = COMBRD;
	lcs = UART_LCS;


#ifdef COMPRESERVE
	lcs = inb(COMCONSOLE + UART_LCR) & 0x7f;
	outb(0x80 | lcs, COMCONSOLE + UART_LCR);
	divisor = (inb(COMCONSOLE + UART_DLM) << 8) | inb(COMCONSOLE + UART_DLL);
	outb(lcs, COMCONSOLE + UART_LCR);
#endif

	/* Set Baud Rate Divisor to CONSPEED, and test to see if the
	 * serial port appears to be present.
	 */
	outb(0x80 | lcs, COMCONSOLE + UART_LCR);
	outb(0xaa, COMCONSOLE + UART_DLL);
	if (inb(COMCONSOLE + UART_DLL) != 0xaa) 
		goto out;
	outb(0x55, COMCONSOLE + UART_DLL);
	if (inb(COMCONSOLE + UART_DLL) != 0x55)
		goto out;
	outb(divisor & 0xff, COMCONSOLE + UART_DLL);
	if (inb(COMCONSOLE + UART_DLL) != (divisor & 0xff))
		goto out;
	outb(0xaa, COMCONSOLE + UART_DLM);
	if (inb(COMCONSOLE + UART_DLM) != 0xaa) 
		goto out;
	outb(0x55, COMCONSOLE + UART_DLM);
	if (inb(COMCONSOLE + UART_DLM) != 0x55)
		goto out;
	outb((divisor >> 8) & 0xff, COMCONSOLE + UART_DLM);
	if (inb(COMCONSOLE + UART_DLM) != ((divisor >> 8) & 0xff))
		goto out;
	outb(lcs, COMCONSOLE + UART_LCR);
	
	/* disable interrupts */
	outb(0x0, COMCONSOLE + UART_IER);

	/* disable fifo's */
	outb(0x00, COMCONSOLE + UART_FCR);

	/* Set clear to send, so flow control works... */
	outb((1<<1), COMCONSOLE + UART_MCR);


	/* Flush the input buffer. */
	do {
		/* rx buffer reg
		 * throw away (unconditionally the first time)
		 */
		inb(COMCONSOLE + UART_RBR);
		/* line status reg */
		status = inb(COMCONSOLE + UART_LSR);
	} while(status & 1);
	initialized = 1;
 out:
	found = initialized;
	return initialized;
}
#endif
