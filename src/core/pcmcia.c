/*
 *	pcmcia.c
 *
 *	PCMCIA support routines for etherboot - generic stuff
 *
 *	This code has partly be taken from the linux kernel sources, .../drivers/pcmcia/
 *	Started & put together by
 *		Anselm Martin Hoffmeister
 *		Stockholm Projekt Computer-Service
 *		Sankt Augustin / Bonn, Germany
 *
 *	Distributed under GPL2
 */

/*
 *
 *
 *			******************************
 *			PLEASE DO NOT YET WORK ON THIS
 *			******************************
 *
 *	I'm still fixing it up on every end, so we most probably would interfere
 *	at some point. If there's anything obvious or better, not-so-obvious,
 *	please contact me by e-mail: anselm (AT) hoffmeister (DOT) be   *THANKS*
 */
#include "../include/pcmcia.h"
#include "../include/i82365.h"
#define CODE_STATUS "pre-alpha"
#define	CODE_VERSION "0.1.1"

#include "../include/pcmcia-opts.h"
#include "../include/pcmcia.h"

int	sockets; /* AHTODO: Phase this out! */
u_int	pccsocks;
struct	pccsock_t pccsock[MAXPCCSOCKS];

struct	driver_interact_t driver[] = {
#ifdef	SUPPORT_I82365
	{ I82365, i82365_interfacer, "Intel_82365" },
#endif
};

#define	NUM_DRIVERS (sizeof(driver)/(sizeof(struct driver_interact_t)))

void	sleepticks(int numticks ) {
	int	tmo;
	for (tmo = currticks()+numticks; currticks() < tmo; ) {
                poll_interruptions();
        }
	return;
}

int	pcmcia_init_all(void) {
	u_int i, j, k, l, ui;
	u_int multicard[8];
	u_char * uc, upc;
	if ( PDEBUG > 0 ) printf("Initializing PCMCIA subsystem (code-status: " CODE_STATUS ", Version " CODE_VERSION ")\n");
	if ( PDEBUG > 2 ) {
		printf ( "Supporting %d driver(s): ", NUM_DRIVERS );
		for ( i = 0; i < NUM_DRIVERS; ++i ) {
			printf ( "[%s] ", driver[i].name );
		}
		printf ( "\n" );
	}
	pccsocks = 0;
	sockets = 0;
	// Init all drivers in the driver[] array:
	for ( i = 0; i < NUM_DRIVERS; ++i ) {
		driver[i].f(INIT,0,i,0,0);	// init needs no params. It uses pccsocks and pccsock[].
						// Only i tells it which driver_id itself is.
	}
	for ( i = 0; i < pccsocks; ++i ) {
		if ( PDEBUG > 2 ) {
			printf ( "Socket %d has status [%s]\n", i, (pccsock[i].status == EMPTY? "EMPTY":(pccsock[i].status == HASCARD?"HASCARD":"other")) );
		}
		if  ( pccsock[i].status != HASCARD ) continue;
		if ( 0 != driver[pccsock[i].drivernum].f(MAPATTRMEM,pccsock[i].internalid,MAP_ATTRMEM_TO, MAP_ATTRMEM_LEN,0 ) ) {
			printf ("PCMCIA controller failed to map attribute memory.\n**** SEVERE ERROR CONDITION. Skipping controller.\n" );
			if ( PDEBUG > 2 ) {
				printf ( "<press key. THIS CONDITION SHOULD BE REPORTED!>\n" ); getchar();
			}
			continue;
		}
		// parse configuration information
		uc = ioremap ( MAP_ATTRMEM_TO, MAP_ATTRMEM_LEN );
		pccsock[i].stringoffset = pccsock[i].configoffset = pccsock[i].stringlength = 0;
		pccsock[i].type = 0xff;
		for ( l = 0; l < 8; ++l ) multicard[l] = 0;
		//printf ( "Going to start first loop <key>\n" ); getchar();
		sleepticks(2);
		for ( l = ui = 0; ui >= 0; ui += uc[(2*ui)+2] + 2 ) {
			if ( uc[(2*ui)] == 0xff ) {
				break;
			}
			if ( ui >= 0x800 ) { break; }
			// This loop is complete rubbish AFAICS.
			// But without it, my test system won't come up.
			// It's too bad to develop on broken hardware
			//				- Anselm
		}
		sleepticks(2);
		for ( l = ui = 0; ui < 0x800; ui += uc[(2*ui)+2] + 2 ) {
			if ( uc[(2*ui)] == 0xff ) break;
			else if ( uc[2*ui] == 0x15 ) {
				for ( k = 2 * ( ui + 2 ); ( uc[k] <= ' ' ) && ( k < ( 2 * ( uc[2*(ui+1)] + ui + 2 ) ) ) ; k += 2 ) { ; }
				pccsock[i].stringoffset = k;
				pccsock[i].stringlength = ( 2 * ( ui + 2 + uc[(2*ui)+2] ) - k ) / 2;
			} else if ( uc[2*ui] == 0x21 ) {
				pccsock[i].type = uc[(2*ui)+4];
			}
		}
		if ( pccsock[i].stringoffset > 0 ) {	// If no identifier, it's not a valid CIS (as of documentation...)
			printf ( "Slot %d: [", i );
			for ( k = 0; ( k < pccsock[i].stringlength ) && ( k < 64 ); ++k ) {
				j = uc[pccsock[i].stringoffset + 2 * k];
				printf ( "%c", (j>=' '? j:' ' ) );
			}
			printf ("]\n        >identified as type %d (", pccsock[i].type );
			switch ( pccsock[i].type ) {
			  case	0x00:
				printf ( "MULTI" ); break;
			  case	0x01:
				printf ( "Memory" ); break;
			  case	0x02:
				printf ( "Serial" ); break;
			  case	0x03:
				printf ( "Parallel" ); break;
			  case	0x04:
				printf ( "Fixed" ); break;
			  case	0x05:
				printf ( "Video" ); break;
			  case	0x06:
				printf ( "Network" ); break;
			  case	0x07:
				printf ( "AIMS" ); break;
			  case	0x08:
				printf ( "SCSI" ); break;
			  case	0x106: // Special / homebrew to say "Multi/network"
				printf ( "MULTI, with Network" ); break; // AHTODO find a card for this
			  default:
				printf ( "UNSUPPORTED/UNKNOWN" );
			}
			printf ( ")\n" );
			// Now set dependency: If it's Network or multi->network, accept
		}
		// printf ("---Done memwin parse   <key>\n" ); getchar();
		// unmap the PCMCIA device
		if ( 0 != driver[pccsock[i].drivernum].f(UNMAPATTRMEM,pccsock[i].internalid,0,0,0) ) {
			printf ("PCMCIA controller failed to unmap attribute memory.\n**** SEVERE ERROR CONDITION ****\n" );
			if ( PDEBUG > 2 ) {
				printf ( "<press key. THIS CONDITION SHOULD BE REPORTED!>\n" ); getchar();
			}
			continue;
		}
	}
	if ( PDEBUG > 2 ) {
		printf ( "<press key to exit the pcmcia_init_all routine>\n" );
		getchar();
	}

	return 0;
}

int	pcmcia_shutdown_all(void) {
	printf("Shutting down PCMCIA subsystem\n");
	//if ( PDEBUG > 2 ) {printf("<press key to continue>\n" ); getchar(); }
	return 0;
}

