/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
UNDI NIC driver for Etherboot

This file copyright (C) Michael Brown <mbrown@fensystems.co.uk> 2003.
All rights reserved.

$Id$
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the PCI support functions, if this is a PCI NIC */
#include "pci.h"
/* UNDI and PXE defines.  Includes pxe.h. */
#include "undi.h"

/* NIC specific static variables go here */
static undi_t undi = { NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0 };

/* Function prototypes */
int undi_full_shutdown ( void );

/**************************************************************************
 * Utility functions
 **************************************************************************/

/* Checksum a block.
 */

uint8_t checksum ( void *block, size_t size ) {
	uint8_t sum = 0;
	uint16_t i = 0;
	for ( i = 0; i < size; i++ ) {
		sum += ( ( uint8_t * ) block )[i];
	}
	return sum;
}

/**************************************************************************
 * Base memory scanning functions
 **************************************************************************/

/* Locate the $PnP structure indicating a PnP BIOS.
 */

int hunt_pnp_bios ( void ) {
	uint32_t off = 0x10000;

	printf ( "Hunting for PnP BIOS..." );
	while ( off > 0 ) {
		off -= 16;
		undi.pnp_bios = (pnp_bios_t *) phys_to_virt ( 0xf0000 + off );
		if ( undi.pnp_bios->signature == PNP_BIOS_SIGNATURE ) {
			printf ( "found $PnP at f000:%hx...", off );
			if ( checksum(undi.pnp_bios,sizeof(pnp_bios_t)) !=0) {
				printf ( "invalid checksum\n..." );
				continue;
			}
			printf ( "ok\n" );
			return 1;
		}
	}
	printf ( "none found\n" );
	return 0;
}

/* Locate the !PXE structure indicating a loaded UNDI driver.
 */

int hunt_pixie ( void ) {
	static uint32_t ptr = 0;

	printf ( "Hunting for pixies..." );
	if ( ptr == 0 ) ptr = 0xa0000;
	while ( ptr > 0x10000 ) {
		ptr -= 16;
		undi.pxe = (pxe_t *) phys_to_virt ( ptr );
		if ( memcmp ( undi.pxe->Signature, "!PXE", 4 ) == 0 ) {
			printf ( "found !PXE at %x...", ptr );
			if ( checksum ( undi.pxe, sizeof(pxe_t) ) != 0 ) {
				printf ( "invalid checksum\n..." );
				continue;
			}
			printf ( "ok\n" );
			return 1;
		}
	}
	printf ( "none found\n" );
	ptr = 0;
	return 0;
}

/* Locate PCI PnP ROMs.
 */

int hunt_rom ( void ) {
	static uint32_t ptr = 0;

	printf ( "Hunting for ROMs..." );
	if ( ptr == 0 ) ptr = 0x100000;
	while ( ptr > 0x0c0000 ) {
		ptr -= 0x800;
		undi.rom = ( rom_t * ) phys_to_virt ( ptr );
		if ( undi.rom->signature == ROM_SIGNATURE ) {
			pcir_header_t *pcir_header = NULL;
			pnp_header_t *pnp_header = NULL;

			printf ( "found 55AA at %x...", ptr );
			if ( undi.rom->pcir_off == 0 ) {
				printf ( "not a PCI ROM\n..." );
				continue;
			}
			pcir_header = (pcir_header_t*)( ( void * ) undi.rom +
							undi.rom->pcir_off );
			if ( pcir_header->signature != PCIR_SIGNATURE ) {
				printf ( "invalid PCI signature\n..." );
				continue;
			}
			printf ( "PCI:%hx:%hx...", pcir_header->vendor_id,
				 pcir_header->device_id );
			if ( undi.rom->pnp_off == 0 ) {
				printf ( "not a PnP ROM\n..." );
				continue;
			}
			pnp_header = (pnp_header_t*)( ( void * ) undi.rom +
							 undi.rom->pnp_off );
			if ( pnp_header->signature != PNP_SIGNATURE ) {
				printf ( "invalid $PnP signature\n..." );
				continue;
			}
			if ( checksum(pnp_header,sizeof(pnp_header_t)) != 0 ) {
				printf ( "invalid PnP checksum\n..." );
				continue;
			}
			printf ( "ok\nROM contains %s by %s\n",
				 pnp_header->product_str_off==0 ? "(unknown)" :
				 (void*)undi.rom+pnp_header->product_str_off,
				 pnp_header->manuf_str_off==0 ? "(unknown)" :
				 (void*)undi.rom+pnp_header->manuf_str_off );
			return 1;
		}
	}
	printf ( "none found\n" );
	ptr = 0;
	return 0;
}

/* Locate ROMs containing UNDI drivers.
 */

int hunt_undi_rom ( void ) {
	while ( hunt_rom() ) {
		if ( undi.rom->undi_rom_id_off == 0 ) {
			printf ( "Not a PXE ROM\n" );
			continue;
		}
		undi.undi_rom_id = (undi_rom_id_t *)
			( (void *)undi.rom + undi.rom->undi_rom_id_off );
		if ( undi.undi_rom_id->signature != UNDI_SIGNATURE ) {
			printf ( "Invalid UNDI signature\n" );
			continue;
		}
		printf ( "Located UNDI ROM supporting revision %d.%d.%d\n",
			 undi.undi_rom_id->undi_rev[2],
			 undi.undi_rom_id->undi_rev[1],
			 undi.undi_rom_id->undi_rev[0] );
		return 1;
	}
	return 0;
}

/**************************************************************************
 * Low-level UNDI API call wrappers
 **************************************************************************/

/* Make a real-mode UNDI API call to the UNDI routine at
 * routine_seg:routine_off, passing in three uint16 parameters on the
 * real-mode stack.
 * Calls the assembler wrapper routine __undi_call.
 */

static inline PXENV_EXIT_t _undi_call ( uint16_t routine_seg,
					uint16_t routine_off, uint16_t st0,
					uint16_t st1, uint16_t st2 ) {
	undi.undi_call_info->routine.segment = routine_seg;
	undi.undi_call_info->routine.offset = routine_off;
	undi.undi_call_info->stack[0] = st0;
	undi.undi_call_info->stack[1] = st1;
	undi.undi_call_info->stack[2] = st2;
	return __undi_call ( virt_to_phys ( undi.undi_call_info ) >> 4,
			     virt_to_phys ( undi.undi_call_info ) & 0xf );
}

/* Make a real-mode call to the UNDI loader routine at
 * routine_seg:routine_off, passing in the seg:off address of a
 * pxenv_structure on the real-mode stack.
 */

int undi_call_loader ( void ) {
	PXENV_EXIT_t pxenv_exit = PXENV_EXIT_FAILURE;
	
	pxenv_exit = _undi_call ( virt_to_phys (undi.rom ) >> 4,
				  undi.undi_rom_id->undi_loader_off,
				  virt_to_phys ( undi.pxs ) & 0xf,
				  virt_to_phys ( undi.pxs ) >> 4,
				  0 /* Unused for UNDI loader API */ );
	/* Return 1 for success, to be consistent with other routines */
	if ( pxenv_exit == PXENV_EXIT_SUCCESS ) return 1;
	printf ( "UNDI loader call failed with status %#hx\n",
		 undi.pxs->Status );
	return 0;
}

/* Make a real-mode UNDI API call, passing in the opcode and the
 * seg:off address of a pxenv_structure on the real-mode stack.
 */

int undi_call ( uint16_t opcode ) {
	PXENV_EXIT_t pxenv_exit = PXENV_EXIT_FAILURE;

	pxenv_exit = _undi_call ( undi.pxe->EntryPointSP.segment,
				  undi.pxe->EntryPointSP.offset,
				  opcode,
				  virt_to_phys ( undi.pxs ) & 0xf,
				  virt_to_phys ( undi.pxs ) >> 4 );
	/* Return 1 for success, to be consistent with other routines */
	if ( pxenv_exit == PXENV_EXIT_SUCCESS ) return 1;
	printf ( "UNDI API call %#hx failed with status %#hx\n",
		 opcode, undi.pxs->Status );
	return 0;
}

/**************************************************************************
 * High-level UNDI API call wrappers
 **************************************************************************/

/* Install the UNDI driver from a located UNDI ROM.
 */

int undi_loader ( void ) {
	undi.pxs->loader.ax = 0; /* UNKNOWN */
	undi.pxs->loader.bx = 0xffff; /* Assume not an ISAPnP device */ 
	undi.pxs->loader.dx = 0xffff; /* Assume not an ISAPnP device */

	undi.pxs->loader.es = 0xf000; /* Pointer to PnP BIOS' $PnP structure */
	undi.pxs->loader.di = virt_to_phys ( undi.pnp_bios ) - 0xf0000;

	undi.pxs->loader.undi_ds = UNDI_DRIVER_DATA_SEGMENT;
	undi.pxs->loader.undi_cs = UNDI_DRIVER_CODE_SEGMENT;

	if ( ! undi_call_loader () ) {
		return 0;
	}
	undi.pxe = phys_to_virt ( ( undi.pxs->loader.undi_cs << 4 ) +
				  undi.pxs->loader.pxe_off );
	printf ( "UNDI driver created a pixie at %hx:%hx...",
		 undi.pxs->loader.undi_cs, undi.pxs->loader.pxe_off );
	if ( memcmp ( undi.pxe->Signature, "!PXE", 4 ) != 0 ) {
		printf ( "invalid signature\n" );
		return 0;
	}
	if ( checksum ( undi.pxe, sizeof(pxe_t) ) != 0 ) {
		printf ( "invalid checksum\n" );
		return 0;
	}
	printf ( "ok\n" );
	undi.loaded = 1;
	return 1;
}

/* Start the UNDI driver.
 */

int pxenv_start_undi ( void ) {
	undi.pxs->start_undi.ax = 0; /* UNKNOWN */
	undi.pxs->start_undi.bx = 0xffff; /* Assume not an ISAPnP device */
	undi.pxs->start_undi.dx = 0xffff; /* Assume not an ISAPnP device */
	
	/* Pointer to PnP BIOS' $PnP structure */
	undi.pxs->start_undi.es = 0xf000;
	undi.pxs->start_undi.di = virt_to_phys ( undi.pnp_bios )-0xf0000;
	
	if ( ! undi_call ( PXENV_START_UNDI ) ) return 0;
	undi.prestarted = 1;
	return 1;
}

int pxenv_undi_startup ( void )	{
	if ( ! undi_call ( PXENV_UNDI_STARTUP ) ) return 0;
	undi.started = 1;
	return 1;
}

int pxenv_undi_cleanup ( void ) { return undi_call ( PXENV_UNDI_CLEANUP ); }

int pxenv_undi_initialize ( void ) {
	undi.pxs->undi_initialize.ProtocolIni = 0;
	memset ( &undi.pxs->undi_initialize.reserved, 0,
		 sizeof ( undi.pxs->undi_initialize.reserved ) );
	if ( ! undi_call ( PXENV_UNDI_INITIALIZE ) ) return 0;
	undi.initialized = 1;
	return 1;
}

int pxenv_undi_shutdown ( void ) {
	if ( ! undi_call ( PXENV_UNDI_SHUTDOWN ) ) return 0;
	undi.initialized = 0;
	undi.started = 0;
	return 1;
}

int pxenv_undi_close ( void ) {
	if ( ! undi_call ( PXENV_UNDI_CLOSE ) ) return 0;
	undi.opened = 0;
	return 1;
}

int pxenv_undi_get_information ( void ) {
	memset ( undi.pxs, 0, sizeof ( undi.pxs ) );
	return undi_call ( PXENV_UNDI_GET_INFORMATION );
}

int pxenv_stop_undi ( void ) {
	if ( ! undi_call ( PXENV_STOP_UNDI ) ) return 0;
	undi.prestarted = 0;
	return 1;
}

int pxenv_unload_stack ( void ) {
	memset ( undi.pxs, 0, sizeof ( undi.pxs ) );	
	if ( ! undi_call ( PXENV_UNLOAD_STACK ) ) return 0;
	undi.loaded = 0;
	return 1;
}

/* UNDI full initialization
 * This calls all the various UNDI initialization routines in sequence.
 */

int undi_full_startup ( void ) {
	if ( pxenv_start_undi() ) {
		if ( pxenv_undi_startup() ) {
			if ( pxenv_undi_initialize() ) {
				return 1;
			}
		}
	}
	undi_full_shutdown();
	return 0;
}

/* UNDI full shutdown
 * This calls all the various UNDI shutdown routines in sequence.  It does
 * not unload the PXE stack.
 */

int undi_full_shutdown ( void ) {
	/* Ignore errors and continue in the hope of shutting down anyway */
	if ( undi.opened ) pxenv_undi_close();
	if ( undi.started ) { 
		pxenv_undi_cleanup();
		pxenv_undi_shutdown();
	}
	if ( undi.prestarted ) pxenv_stop_undi();
	if ( undi.loaded ) pxenv_unload_stack();
	return 1;
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int undi_poll(struct nic *nic)
{
	/* return true if there's an ethernet packet ready to read */
	/* nic->packet should contain data on return */
	/* nic->packetlen should contain length of data */
	return (0);	/* initially as this is called to flush the input */
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void undi_transmit(
	struct nic *nic,
	const char *d,			/* Destination */
	unsigned int t,			/* Type */
	unsigned int s,			/* size */
	const char *p)			/* Packet */
{
	/* send the packet to destination */
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void undi_disable(struct dev *dev)
{
	/* put the card in its initial state */
	/* This function serves 3 purposes.
	 * This disables DMA and interrupts so we don't receive
	 *  unexpected packets or interrupts from the card after
	 *  etherboot has finished. 
	 * This frees resources so etherboot may use
	 *  this driver on another interface
	 * This allows etherboot to reinitialize the interface
	 *  if something is something goes wrong.
	 */
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/

/* Locate an UNDI driver by first scanning through base memory for an
 * installed driver and then by scanning for UNDI ROMs and attempting
 * to install their drivers.
 */

int hunt_pixies_and_undi_roms ( void ) {
	static uint8_t hunt_type = HUNT_FOR_PIXIES;
	
	if ( hunt_type == HUNT_FOR_PIXIES ) {
		if ( hunt_pixie() ) {
			return 1;
		}
	}
	hunt_type = HUNT_FOR_UNDI_ROMS;
	while ( hunt_undi_rom() ) {
		if ( undi_loader() ) {
			return 1;
		}
	}
	hunt_type = HUNT_FOR_PIXIES;
	return 0;
}

/* The actual Etherboot probe routine.
 */

#ifdef RELOCATE
#error UNDI driver does not yet work with relocation
#endif

static int undi_probe(struct dev *dev, struct pci_device *pci)
{
	struct nic *nic = (struct nic *)dev;

	/* We need to allocate space in base memory for undi_call_info
	 * and pxs.  This is a hack that will fail when relocation is
	 * in use.
	 */
	static undi_call_info_t HACK_undi_call_info;
	static pxenv_structure_t HACK_pxs;
	undi.undi_call_info = &HACK_undi_call_info;
	undi.pxs = &HACK_pxs;

	if ( ! hunt_pnp_bios() ) {
		printf ( "No PnP BIOS found; aborting\n" );
		return 0;
	}

	for ( ; hunt_pixies_and_undi_roms(); undi_full_shutdown() ) {
		if ( ! undi_full_startup() ) continue;
		printf ( "Good!  We managed to initialize everything!\n" );

		if ( ! pxenv_undi_get_information() ) continue;

		/* MAC address & other info not being returned properly yet */
		printf ( "Initialized UNDI NIC with IO %#hx, IRQ %d, MAC %!\n",
			 undi.pxs->undi_get_information.BaseIo,
			 undi.pxs->undi_get_information.IntNumber,
			 undi.pxs->undi_get_information.CurrentNodeAddress );
		printf ( "MTU %d, RX buffer size %d, TX buffer size %d\n",
			 undi.pxs->undi_get_information.MaxTranUnit,
			 undi.pxs->undi_get_information.RxBufCt,
			 undi.pxs->undi_get_information.TxBufCt );
	}
	
#define board_found 1
#define valid_link 0

	if (board_found && valid_link)
	{
		/* point to NIC specific routines */
		dev->disable  = undi_disable;
		nic->poll     = undi_poll;
		nic->transmit = undi_transmit;
		return 1;
	}
	/* else */
	return 0;
}

/* Hack - this is currently only a proof-of-concept driver, so we
 * cheat and tell it to work with the card I have available for
 * testing, rather than bothering to hack around the bus scanning
 * routines.
 */
static struct pci_id undi_nics[] = {
PCI_ROM(0x10ec, 0x8139, "undi", "UNDI adaptor"),
};

static struct pci_driver undi_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "UNDI",
	.probe    = undi_probe,
	.ids      = undi_nics,
	.id_count = sizeof(undi_nics)/sizeof(undi_nics[0]),
	.class    = 0,
};
