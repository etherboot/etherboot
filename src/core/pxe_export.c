/* PXE API interface for Etherboot.
 *
 * Copyright (C) 2004 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Tags used in this file:
 *
 * FIXME : obvious
 * PXESPEC : question over interpretation of the PXE spec.
 */

#ifdef PXE_EXPORT

#include "etherboot.h"
#include "pxe.h"
#include "pxe_export.h"
#include "pxe_callbacks.h"
#include "nic.h"
#include "pci.h"
#include "dev.h"

#if TRACE_PXE
#define DBG(...) printf ( __VA_ARGS__ )
#else
#define DBG(...)
#endif

/* Global pointer to currently installed PXE stack */
pxe_stack_t *pxe_stack = NULL;

/* Various startup/shutdown routines.  The startup/shutdown call
 * sequence is incredibly badly defined in the Intel PXE spec, for
 * example:
 *
 *   PXENV_UNDI_INITIALIZE says that the parameters used to initialize
 *   the adaptor should be those supplied to the most recent
 *   PXENV_UNDI_STARTUP call.  PXENV_UNDI_STARTUP takes no parameters.
 *
 *   PXENV_UNDI_CLEANUP says that the rest of the API will not be
 *   available after making this call.  Figure 3-3 ("Early UNDI API
 *   usage") shows a call to PXENV_UNDI_CLEANUP being followed by a
 *   call to the supposedly now unavailable PXENV_STOP_UNDI.
 *
 *   PXENV_UNLOAD_BASE_STACK talks about freeing up the memory
 *   occupied by the PXE stack.  Figure 4-3 ("PXE IPL") shows a call
 *   to PXENV_STOP_UNDI being made after the call to
 *   PXENV_UNLOAD_BASE_STACK, by which time the entire PXE stack
 *   should have been freed (and, potentially, zeroed).
 *
 *   Nothing, anywhere, seems to mention who's responsible for freeing
 *   up the base memory allocated for the stack segment.  It's not
 *   even clear whether or not this is expected to be in free base
 *   memory rather than claimed base memory.
 *
 * Consequently, we adopt a rather defensive strategy, designed to
 * work with any conceivable sequence of initialisation or shutdown
 * calls.  We have only two things that we care about:
 *
 *   1. Have we hooked INT 1A and INT 15,E820(etc.)?
 *   2. Is the NIC initialised?
 *
 * The NIC should never be initialised without the vectors being
 * hooked, similarly the vectors should never be unhooked with the NIC
 * still initialised.  We do, however, want to be able to have the
 * vectors hooked with the NIC shutdown.  We therefore have three
 * possible states:
 *
 *   1. Ready to unload: interrupts unhooked, NIC shutdown.
 *   2. Midway: interrupts hooked, NIC shutdown.
 *   3. Fully ready: interrupts hooked, NIC initialised.
 *
 * We provide the three states CAN_UNLOAD, MIDWAY and READY to define
 * these, and the call pxe_ensure_state() to ensure that the stack is
 * in the specified state.  All our PXE API call implementations
 * should use this call to ensure that the state is as required for
 * that PXE API call.  This enables us to cope with whatever the
 * end-user's interpretation of the PXE spec may be.  It even allows
 * for someone calling e.g. PXENV_START_UNDI followed by
 * PXENV_UDP_WRITE, without bothering with any of the intervening
 * calls.
 *
 * pxe_ensure_state() returns 1 for success, 0 for failure.  In the
 * event of failure (which can arise from e.g. asking for state READY
 * when we don't know where our NIC is), the error code
 * PXENV_STATUS_UNDI_INVALID_STATE should be returned to the user.
 * The macros ENSURE_XXX() can be used to achieve this without lots of
 * duplicated code.
 */

/* pxe_[un]hook_stack are architecture-specific and provided in
 * pxe_callbacks.c
 */

/* Dummy PCI driver.  This is used in order to hack the probe
 * mechanism; we need to be able to set dev.driver != NULL to fool it
 * into not starting from the beginning, so we need somewhere to point
 * dev.driver.
 *
 * __pci_driver is deliberately omitted so that this *doesn't* show up
 * in the normal drivers list.
 */
static struct pci_driver dummy_driver = {
        .type     = NIC_DRIVER,
        .name     = "DUMMY",
        .probe    = NULL,
        .ids      = NULL,
        .id_count = 0,
        .class    = 0,
};

int pxe_initialise_nic ( void ) {
	if ( pxe_stack->state >= READY ) return 1;

	/* Check if NIC is initialised.  nic.dev.disable is set to 0
	 * when disable() is called, so we use this.
	 */
	if ( nic.dev.disable ) {
		/* NIC may have been initialised independently
		 * (e.g. when we set up the stack prior to calling the
		 * NBP).
		 */
		pxe_stack->state = READY;
		return 1;
	}

	/* If we already have a NIC defined, reuse that one with
	 * PROBE_AWAKE.  If one was specifed via PXENV_START_UNDI, try
	 * that one first.  Otherwise, set PROBE_FIRST.
	 */
	if ( nic.dev.state.pci.dev.driver == &dummy_driver ) {
		nic.dev.how_probe = PROBE_NEXT;
	} else if ( nic.dev.state.pci.dev.driver ) {
		nic.dev.how_probe = PROBE_AWAKE;
	} else {
		nic.dev.how_probe = PROBE_FIRST;
	}
	
	/* Call probe routine to bring up the NIC */
	if ( eth_probe ( &nic.dev ) != PROBE_WORKED ) {
		return 0;
	}
	pxe_stack->state = READY;
	return 1;
}

int pxe_shutdown_nic ( void ) {
	if ( pxe_stack->state <= MIDWAY ) return 1;

	eth_disable();
	pxe_stack->state = MIDWAY;
	return 1;
}

int ensure_pxe_state ( pxe_stack_state_t wanted ) {
	int success = 1;

	if ( ! pxe_stack ) return 0;
	if ( wanted >= MIDWAY )
		success = success & hook_pxe_stack();
	if ( wanted > MIDWAY ) {
		success = success & pxe_initialise_nic();
	} else {
		success = success & pxe_shutdown_nic();
	}
	if ( wanted < MIDWAY )
		success = success & unhook_pxe_stack();
	return success;
}

#define ENSURE_CAN_UNLOAD(structure) if ( ! ensure_pxe_state(CAN_UNLOAD) ) { \
			structure->Status = PXENV_STATUS_UNDI_INVALID_STATE; \
			return PXENV_EXIT_FAILURE; }
#define ENSURE_MIDWAY(structure) if ( ! ensure_pxe_state(MIDWAY) ) { \
			structure->Status = PXENV_STATUS_UNDI_INVALID_STATE; \
			return PXENV_EXIT_FAILURE; }
#define ENSURE_READY(structure) if ( ! ensure_pxe_state(READY) ) { \
			structure->Status = PXENV_STATUS_UNDI_INVALID_STATE; \
			return PXENV_EXIT_FAILURE; }

/*****************************************************************************
 *
 * Actual PXE API calls
 *
 *****************************************************************************/

/* PXENV_START_UNDI
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_start_undi ( t_PXENV_START_UNDI *start_undi ) {
	unsigned char bus, devfn;
	struct pci_probe_state *pci = &nic.dev.state.pci;

	DBG ( "PXENV_START_UNDI" );
	ENSURE_MIDWAY(start_undi);

	/* Record PCI bus & devfn passed by caller, so we know which
	 * NIC they want to use.
	 *
	 * If they don't match our already-existing NIC structure, set
	 * values to ensure that the specified NIC is used at the next
	 * call to pxe_intialise_nic().
	 */
	bus = ( start_undi->ax >> 8 ) & 0xff;
	devfn = start_undi->ax & 0xff;

	if ( ( pci->dev.driver == NULL ) ||
	     ( pci->dev.bus != bus ) || ( pci->dev.devfn != devfn ) ) {
		/* This is quite a bit of a hack and relies on
		 * knowledge of the internal operation of Etherboot's
		 * probe mechanism.
		 */
		DBG ( " set PCI %hhx:%hhx.%hhx",
		      bus, PCI_SLOT(devfn), PCI_FUNC(devfn) );
		pci->advance = 1;
		pci->dev.driver = &dummy_driver;
		pci->dev.bus = bus;
		pci->dev.devfn = devfn;
	}

	start_undi->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_STARTUP
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_startup ( t_PXENV_UNDI_STARTUP *undi_startup ) {
	DBG ( "PXENV_UNDI_STARTUP" );
	ENSURE_MIDWAY(undi_startup);

	undi_startup->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_CLEANUP
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_cleanup ( t_PXENV_UNDI_CLEANUP *undi_cleanup ) {
	DBG ( "PXENV_UNDI_CLEANUP" );
	ENSURE_CAN_UNLOAD ( undi_cleanup );

	undi_cleanup->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_INITIALIZE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_initialize ( t_PXENV_UNDI_INITIALIZE
				     *undi_initialize ) {
	DBG ( "PXENV_UNDI_INITIALIZE" );
	ENSURE_MIDWAY ( undi_initialize );

	undi_initialize->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_RESET_ADAPTER
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_reset_adapter ( t_PXENV_UNDI_RESET_ADAPTER
					*undi_reset_adapter ) {
	DBG ( "PXENV_UNDI_RESET_ADAPTER" );

	ENSURE_MIDWAY ( undi_reset_adapter );
	ENSURE_READY ( undi_reset_adapter );

	undi_reset_adapter->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_SHUTDOWN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_shutdown ( t_PXENV_UNDI_SHUTDOWN *undi_shutdown ) {
	DBG ( "PXENV_UNDI_SHUTDOWN" );
	ENSURE_MIDWAY ( undi_shutdown );

	undi_shutdown->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_OPEN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_open ( t_PXENV_UNDI_OPEN *undi_open ) {
	DBG ( "PXENV_UNDI_OPEN" );
	ENSURE_READY ( undi_open );

	undi_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_CLOSE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_close ( t_PXENV_UNDI_CLOSE *undi_close ) {
	DBG ( "PXENV_UNDI_CLOSE" );
	ENSURE_MIDWAY ( undi_close );

	undi_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_TRANSMIT
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_transmit ( t_PXENV_UNDI_TRANSMIT *undi_transmit ) {
	DBG ( "PXENV_UNDI_TRANSMIT" );
	ENSURE_READY ( undi_transmit );

	undi_transmit->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_SET_MCAST_ADDRESS
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_set_mcast_address ( t_PXENV_UNDI_SET_MCAST_ADDRESS
					    *undi_set_mcast_address ) {
	DBG ( "PXENV_UNDI_SET_MCAST_ADDRESS" );
	/* ENSURE_READY ( undi_set_mcast_address ); */
	undi_set_mcast_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_SET_STATION_ADDRESS
 *
 * Status: working (deliberately incomplete)
 */
PXENV_EXIT_t pxenv_undi_set_station_address ( t_PXENV_UNDI_SET_STATION_ADDRESS
					      *undi_set_station_address ) {
	DBG ( "PXENV_UNDI_SET_STATION_ADDRESS" );
	ENSURE_READY ( undi_set_station_address );

	/* We don't offer a facility to set the MAC address; this
	 * would require adding extra code to all the Etherboot
	 * drivers, for very little benefit.  If we're setting it to
	 * the current value anyway then return success, otherwise
	 * return UNSUPPORTED.
	 */
	if ( memcmp ( nic.node_addr,
		      &undi_set_station_address->StationAddress,
		      ETH_ALEN ) == 0 ) {
		undi_set_station_address->Status = PXENV_STATUS_SUCCESS;
		return PXENV_EXIT_SUCCESS;
	}
	undi_set_station_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_SET_PACKET_FILTER
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_set_packet_filter ( t_PXENV_UNDI_SET_PACKET_FILTER
					    *undi_set_packet_filter ) {
	DBG ( "PXENV_UNDI_SET_PACKET_FILTER" );
	/* ENSURE_READY ( undi_set_packet_filter ); */
	undi_set_packet_filter->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_INFORMATION
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_information ( t_PXENV_UNDI_GET_INFORMATION
					  *undi_get_information ) {
	DBG ( "PXENV_UNDI_GET_INFORMATION" );
	ENSURE_READY ( undi_get_information );

	undi_get_information->BaseIo = nic.ioaddr;
	undi_get_information->IntNumber = nic.irqno;
	/* Cheat: assume all cards can cope with this */
	undi_get_information->MaxTranUnit = ETH_MAX_MTU;
	/* Cheat: we only ever have Ethernet cards */
	undi_get_information->HwType = ETHER_TYPE;
	undi_get_information->HwAddrLen = ETH_ALEN;
	/* Cheat: assume card is always configured with its permanent
	 * node address.  This is a valid assumption within Etherboot
	 * at the time of writing.
	 */
	memcpy ( &undi_get_information->CurrentNodeAddress, nic.node_addr,
		 ETH_ALEN );
	memcpy ( &undi_get_information->PermNodeAddress, nic.node_addr,
		 ETH_ALEN );
	undi_get_information->ROMAddress = nic.rom_info->rom_segment;
	/* We only provide the ability to receive or transmit a single
	 * packet at a time.  This is a bootloader, not an OS.
	 */
	undi_get_information->RxBufCt = 1;
	undi_get_information->TxBufCt = 1;
	undi_get_information->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_GET_STATISTICS
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_get_statistics ( t_PXENV_UNDI_GET_STATISTICS
					 *undi_get_statistics ) {
	DBG ( "PXENV_UNDI_GET_STATISTICS" );
	/* ENSURE_READY ( undi_get_statistics ); */
	undi_get_statistics->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_CLEAR_STATISTICS
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_clear_statistics ( t_PXENV_UNDI_CLEAR_STATISTICS
					   *undi_clear_statistics ) {
	DBG ( "PXENV_UNDI_CLEAR_STATISTICS" );
	/* ENSURE_READY ( undi_clear_statistics ); */
	undi_clear_statistics->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_INITIATE_DIAGS
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_initiate_diags ( t_PXENV_UNDI_INITIATE_DIAGS
					 *undi_initiate_diags ) {
	DBG ( "PXENV_UNDI_INITIATE_DIAGS" );
	/* ENSURE_READY ( undi_initiate_diags ); */
	undi_initiate_diags->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_FORCE_INTERRUPT
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_force_interrupt ( t_PXENV_UNDI_FORCE_INTERRUPT
					  *undi_force_interrupt ) {
	DBG ( "PXENV_UNDI_FORCE_INTERRUPT" );
	/* ENSURE_READY ( undi_force_interrupt ); */
	undi_force_interrupt->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_MCAST_ADDRESS
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_get_mcast_address ( t_PXENV_UNDI_GET_MCAST_ADDRESS
					    *undi_get_mcast_address ) {
	DBG ( "PXENV_UNDI_GET_MCAST_ADDRESS" );
	/* ENSURE_READY ( undi_get_mcast_address ); */
	undi_get_mcast_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_NIC_TYPE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_nic_type ( t_PXENV_UNDI_GET_NIC_TYPE
				       *undi_get_nic_type ) {
	struct dev *dev = &nic.dev;
	
	DBG ( "PXENV_UNDI_GET_NIC_TYPE" );
	ENSURE_READY ( undi_get_nic_type );
	
	if ( dev->to_probe == PROBE_PCI ) {
		struct pci_device *pci = &dev->state.pci.dev;

		undi_get_nic_type->NicType = PCI_NIC;
		undi_get_nic_type->info.pci.Vendor_ID = pci->vendor;
		undi_get_nic_type->info.pci.Dev_ID = pci->dev_id;
		undi_get_nic_type->info.pci.Base_Class = pci->class >> 8;
		undi_get_nic_type->info.pci.Sub_Class = pci->class & 0xff;
		undi_get_nic_type->info.pci.BusDevFunc =
			( pci->bus << 8 ) | pci->devfn;
		/* Cheat: these fields are probably unnecessary, and
		 * would require adding extra code to pci.c.
		 */
		undi_get_nic_type->info.pci.Prog_Intf = 0;
		undi_get_nic_type->info.pci.Rev = 0;
		undi_get_nic_type->info.pci.SubVendor_ID = 0xffff;
		undi_get_nic_type->info.pci.SubDevice_ID = 0xffff;
	} else if ( dev->to_probe == PROBE_ISA ) {
		/* const struct isa_driver *isa = dev->state.isa.driver; */

		undi_get_nic_type->NicType = PnP_NIC;
		/* Don't think anything fills these fields in, and
		 * probably no-one will ever be interested in them.
		 */
		undi_get_nic_type->info.pnp.EISA_Dev_ID = 0;
		undi_get_nic_type->info.pnp.Base_Class = 0;
		undi_get_nic_type->info.pnp.Sub_Class = 0;
		undi_get_nic_type->info.pnp.Prog_Intf = 0;
		undi_get_nic_type->info.pnp.CardSelNum = 0;
	} else {
		/* PXESPEC: There doesn't seem to be an "unknown type"
		 * defined.
		 */
		undi_get_nic_type->NicType = 0;
	}
	undi_get_nic_type->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_GET_IFACE_INFO
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_iface_info ( t_PXENV_UNDI_GET_IFACE_INFO
					 *undi_get_iface_info ) {
	DBG ( "PXENV_UNDI_GET_IFACE_INFO" );
	ENSURE_READY ( undi_get_iface_info );

	/* Just hand back some info, doesn't really matter what it is.
	 * Most PXE stacks seem to take this approach.
	 */
	sprintf ( undi_get_iface_info->IfaceType, "Etherboot" );
	undi_get_iface_info->LinkSpeed = 10000000; /* 10 Mbps */
	undi_get_iface_info->ServiceFlags = 0;
	memset ( undi_get_iface_info->Reserved, 0,
		 sizeof(undi_get_iface_info->Reserved) );
	undi_get_iface_info->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_ISR
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_isr ( t_PXENV_UNDI_ISR *undi_isr ) {
	DBG ( "PXENV_UNDI_ISR" );
	/* We can't call ENSURE_READY, because this could be being
	 * called as part of an interrupt service routine.  Instead,
	 * we should simply die if we're not READY.
	 */
	if ( ( pxe_stack == NULL ) || ( pxe_stack->state < READY ) ) {
		undi_isr->Status = PXENV_STATUS_UNDI_INVALID_STATE;
		return PXENV_EXIT_FAILURE;
	}
	
	undi_isr->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_STOP_UNDI
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_stop_undi ( t_PXENV_STOP_UNDI *stop_undi ) {
	DBG ( "PXENV_STOP_UNDI" );
	ENSURE_CAN_UNLOAD ( stop_undi );

	stop_undi->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_OPEN
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_open ( t_PXENV_TFTP_OPEN *tftp_open ) {
	DBG ( "PXENV_TFTP_OPEN" );
	/* ENSURE_READY ( tftp_open ); */
	tftp_open->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_TFTP_CLOSE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_close ( t_PXENV_TFTP_CLOSE *tftp_close ) {
	DBG ( "PXENV_TFTP_CLOSE" );
	/* ENSURE_READY ( tftp_close ); */
	tftp_close->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_TFTP_READ
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_read ( t_PXENV_TFTP_READ *tftp_read ) {
	DBG ( "PXENV_TFTP_READ" );
	/* ENSURE_READY ( tftp_read ); */
	tftp_read->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_TFTP_READ_FILE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_read_file ( t_PXENV_TFTP_READ_FILE *tftp_read_file ) {
	DBG ( "PXENV_TFTP_READ_FILE" );
	/* ENSURE_READY ( tftp_read_file ); */
	tftp_read_file->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_TFTP_GET_FSIZE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_get_fsize ( t_PXENV_TFTP_GET_FSIZE *tftp_get_fsize ) {
	DBG ( "PXENV_TFTP_GET_FSIZE" );
	/* ENSURE_READY ( tftp_get_fsize ); */
	tftp_get_fsize->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UDP_OPEN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_udp_open ( t_PXENV_UDP_OPEN *udp_open ) {
	DBG ( "PXENV_UDP_OPEN" );
	ENSURE_READY ( udp_open );

	if ( udp_open->src_ip != arptable[ARP_CLIENT].ipaddr.s_addr ) {
		/* Overwrite our IP address */
		DBG ( " with new IP %@", udp_open->src_ip );
		arptable[ARP_CLIENT].ipaddr.s_addr = udp_open->src_ip;
	}

	udp_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UDP_CLOSE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_udp_close ( t_PXENV_UDP_CLOSE *udp_close ) {
	DBG ( "PXENV_UDP_CLOSE" );
	ENSURE_READY ( udp_close );
	udp_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UDP_READ
 *
 * Status: working
 */
int await_pxe_udp ( int ival __unused, void *ptr,
		    unsigned short ptype __unused,
		    struct iphdr *ip, struct udphdr *udp,
		    struct tcphdr *tcp __unused ) {
	t_PXENV_UDP_READ *udp_read = (t_PXENV_UDP_READ*)ptr;
	uint16_t d_port;
	size_t size;

	/* Ignore non-UDP packets */
	if ( !udp ) {
		DBG ( " non-UDP" );
		return 0;
	}
	
	/* Check dest_ip */
	if ( udp_read->dest_ip && ( udp_read->dest_ip != ip->dest.s_addr ) ) {
		DBG ( " wrong dest IP" );
		return 0;
	}

	/* Check dest_port */
	d_port = ntohs ( udp_read->d_port );
	if ( d_port && ( d_port != ntohs(udp->dest) ) ) {
		DBG ( " wrong dest port (got %d, wanted %d)",
		      ntohs(udp->dest), d_port );
		return 0;
	}

	/* Copy packet to buffer and fill in information */
	udp_read->src_ip = ip->src.s_addr;
	udp_read->s_port = udp->src; /* Both in network order */
	size = ntohs(udp->len) - sizeof(*udp);
	if ( udp_read->buffer_size < size ) {
		/* PXESPEC: are we meant to report any kind of error
		 * in this case?
		 */
		size = udp_read->buffer_size;
	}
	memcpy ( SEGOFF16_TO_PTR ( udp_read->buffer ), &udp->payload, size );
	udp_read->buffer_size = size;

	return 1;
}

PXENV_EXIT_t pxenv_udp_read ( t_PXENV_UDP_READ *udp_read ) {
	DBG ( "PXENV_UDP_READ" );
	ENSURE_READY ( udp_read );

	/* Use await_reply with a timeout of zero */
	if ( ! await_reply ( await_pxe_udp, 0, udp_read, 0 ) ) {
		udp_read->Status = PXENV_STATUS_FAILURE;
		return PXENV_EXIT_FAILURE;
	}

	udp_read->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UDP_WRITE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_udp_write ( t_PXENV_UDP_WRITE *udp_write ) {
	uint16_t src_port;
	uint16_t dst_port;
	struct udppacket *packet = (struct udppacket *)nic.packet;
	int packet_size;

	DBG ( "PXENV_UDP_WRITE" );
	ENSURE_READY ( udp_write );

	/* PXE spec says source port is 2069 if not specified */
	src_port = ntohs(udp_write->src_port);
	if ( src_port == 0 ) src_port = 2069;
	dst_port = ntohs(udp_write->dst_port);
	DBG ( " %d->%d (%d)", src_port, dst_port, udp_write->buffer_size );
	
	/* FIXME: we ignore the gateway specified, since we're
	 * confident of being able to do our own routing.  We should
	 * probably allow for multiple gateways.
	 */
	
	/* Copy payload to packet buffer */
	packet_size = ( (void*)&packet->payload - (void*)packet )
		+ udp_write->buffer_size;
	if ( packet_size > ETH_FRAME_LEN ) {
		udp_write->Status = PXENV_STATUS_OUT_OF_RESOURCES;
		return PXENV_EXIT_FAILURE;
	}
	memcpy ( &packet->payload, SEGOFF16_TO_PTR(udp_write->buffer),
		 udp_write->buffer_size );

	/* Transmit packet */
	if ( ! udp_transmit ( udp_write->ip, src_port, dst_port,
			      packet_size, packet ) ) {
		udp_write->Status = PXENV_STATUS_FAILURE;
		return PXENV_EXIT_FAILURE;
	}

	udp_write->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNLOAD_STACK
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_unload_stack ( t_PXENV_UNLOAD_STACK *unload_stack ) {
	DBG ( "PXENV_UNLOAD_STACK" );
	ENSURE_CAN_UNLOAD ( unload_stack );

	/* We need to call cleanup() at some point.  The network card
	 * has already been disabled by ENSURE_CAN_UNLOAD(), but for
	 * the sake of completeness we should call the console_fini()
	 * etc. that are part of cleanup().
	 *
	 * There seems to be a lack of consensus on which is the final
	 * PXE API call to make, but it's a fairly safe bet that all
	 * the potential shutdown sequences will include a call to
	 * PXENV_UNLOAD_STACK at some point, so we may as well do it
	 * here.
	 */
	cleanup();

	unload_stack->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_GET_CACHED_INFO
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_get_cached_info ( t_PXENV_GET_CACHED_INFO
				     *get_cached_info ) {
	BOOTPLAYER *cached_info = &pxe_stack->cached_info;
	DBG ( "PXENV_GET_CACHED_INFO %d", get_cached_info->PacketType );
	ENSURE_READY ( get_cached_info );

	/* Fill in cached_info structure in our pxe_stack */

	/* I don't think there's actually any way we can be called in
	 * the middle of a DHCP request... 
	 */
	cached_info->opcode = BOOTP_REP;
	/* We only have Ethernet drivers */
	cached_info->Hardware = ETHER_TYPE;
	cached_info->Hardlen = ETH_ALEN;
	/* PXESPEC: "Client sets" says the spec, but who's filling in
	 * this structure?  It ain't the client.
	 */
	cached_info->Gatehops = 0;
	cached_info->ident = 0;
	cached_info->seconds = 0;
	cached_info->Flags = BOOTP_BCAST;
	/* PXESPEC: What do 'Client' and 'Your' IP address refer to? */
	cached_info->cip = arptable[ARP_CLIENT].ipaddr.s_addr;
	cached_info->yip = arptable[ARP_CLIENT].ipaddr.s_addr;
	cached_info->sip = arptable[ARP_SERVER].ipaddr.s_addr;
	/* PXESPEC: Does "GIP" mean "Gateway" or "Relay agent"? */
	cached_info->gip = arptable[ARP_GATEWAY].ipaddr.s_addr;
	memcpy ( cached_info->CAddr, arptable[ARP_CLIENT].node, ETH_ALEN );
	/* Nullify server name */
	cached_info->Sname[0] = '\0';
	memcpy ( cached_info->bootfile, KERNEL_BUF,
		 sizeof(cached_info->bootfile) );
	/* Copy DHCP vendor options */
	memcpy ( &cached_info->vendor.d, BOOTP_DATA_ADDR->bootp_reply.bp_vend,
		 sizeof(cached_info->vendor.d) );
	
	/* Copy to user-specified buffer, or set pointer to our buffer */
	get_cached_info->BufferLimit = sizeof(*cached_info);
	/* PXESPEC: says to test for Buffer == NULL *and* BufferSize =
	 * 0, but what are we supposed to do with a null buffer of
	 * non-zero size?!
	 */
	if ( IS_NULL_SEGOFF16 ( get_cached_info->Buffer ) ) {
		/* Point back to our buffer */
		PTR_TO_SEGOFF16 ( cached_info, get_cached_info->Buffer );
		get_cached_info->BufferSize = sizeof(*cached_info);
	} else {
		/* Copy to user buffer */
		size_t size = sizeof(*cached_info);
		void *buffer = SEGOFF16_TO_PTR ( get_cached_info->Buffer );
		if ( get_cached_info->BufferSize < size )
			size = get_cached_info->BufferSize;
		DBG ( " to %x", virt_to_phys ( buffer ) );
		memcpy ( buffer, cached_info, size );
		/* PXESPEC: Should we return an error if the user
		 * buffer is too small?  We do return the actual size
		 * of the buffer via BufferLimit, so the user does
		 * have a way to detect this already.
		 */
	}

	get_cached_info->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_RESTART_TFTP
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_restart_tftp ( t_PXENV_RESTART_TFTP *restart_tftp ) {
	DBG ( "PXENV_RESTART_TFTP" );
	/* ENSURE_READY ( restart_tftp ); */
	restart_tftp->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_START_BASE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_start_base ( t_PXENV_START_BASE *start_base ) {
	DBG ( "PXENV_START_BASE" );
	/* ENSURE_READY ( start_base ); */
	start_base->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_STOP_BASE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_stop_base ( t_PXENV_STOP_BASE *stop_base ) {
	DBG ( "PXENV_STOP_BASE" );
	/* ENSURE_READY ( stop_base ); */
	stop_base->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* API call dispatcher
 *
 * Status: complete
 */
PXENV_EXIT_t pxe_api_call ( int opcode, t_PXENV_ANY *params ) {
	PXENV_EXIT_t ret = PXENV_EXIT_FAILURE;

	/* Set default status in case child routine fails to do so */
	params->Status = PXENV_STATUS_FAILURE;

	DBG ( "[" );

	/* Hand off to relevant API routine */
	switch ( opcode ) {
	case PXENV_START_UNDI:
		ret = pxenv_start_undi ( &params->start_undi );
		break;
	case PXENV_UNDI_STARTUP:
		ret = pxenv_undi_startup ( &params->undi_startup );
		break;
	case PXENV_UNDI_CLEANUP:
		ret = pxenv_undi_cleanup ( &params->undi_cleanup );
		break;
	case PXENV_UNDI_INITIALIZE:
		ret = pxenv_undi_initialize ( &params->undi_initialize );
		break;
	case PXENV_UNDI_RESET_ADAPTER:
		ret = pxenv_undi_reset_adapter ( &params->undi_reset_adapter );
		break;
	case PXENV_UNDI_SHUTDOWN:
		ret = pxenv_undi_shutdown ( &params->undi_shutdown );
		break;
	case PXENV_UNDI_OPEN:
		ret = pxenv_undi_open ( &params->undi_open );
		break;
	case PXENV_UNDI_CLOSE:
		ret = pxenv_undi_close ( &params->undi_close );
		break;
	case PXENV_UNDI_TRANSMIT:
		ret = pxenv_undi_transmit ( &params->undi_transmit );
		break;
	case PXENV_UNDI_SET_MCAST_ADDRESS:
		ret = pxenv_undi_set_mcast_address (
				             &params->undi_set_mcast_address );
		break;
	case PXENV_UNDI_SET_STATION_ADDRESS:
		ret = pxenv_undi_set_station_address (
					   &params->undi_set_station_address );
		break;
	case PXENV_UNDI_SET_PACKET_FILTER:
		ret = pxenv_undi_set_packet_filter (
					     &params->undi_set_packet_filter );
		break;
	case PXENV_UNDI_GET_INFORMATION:
		ret = pxenv_undi_get_information (
					       &params->undi_get_information );
		break;
	case PXENV_UNDI_GET_STATISTICS:
		ret = pxenv_undi_get_statistics (
					        &params->undi_get_statistics );
		break;
	case PXENV_UNDI_CLEAR_STATISTICS:
		ret = pxenv_undi_clear_statistics (
					      &params->undi_clear_statistics );
		break;
	case PXENV_UNDI_INITIATE_DIAGS:
		ret = pxenv_undi_initiate_diags (
						&params->undi_initiate_diags );
		break;
	case PXENV_UNDI_FORCE_INTERRUPT:
		ret = pxenv_undi_force_interrupt (
					       &params->undi_force_interrupt );
		break;
	case PXENV_UNDI_GET_MCAST_ADDRESS:
		ret = pxenv_undi_get_mcast_address (
					     &params->undi_get_mcast_address );
		break;
	case PXENV_UNDI_GET_NIC_TYPE:
		ret = pxenv_undi_get_nic_type ( &params->undi_get_nic_type );
		break;
	case PXENV_UNDI_GET_IFACE_INFO:
		ret = pxenv_undi_get_iface_info (
					        &params->undi_get_iface_info );
		break;
	case PXENV_UNDI_ISR:
		ret = pxenv_undi_isr ( &params->undi_isr );
		break;
	case PXENV_STOP_UNDI:
		ret = pxenv_stop_undi ( &params->stop_undi );
		break;
	case PXENV_TFTP_OPEN:
		ret = pxenv_tftp_open ( &params->tftp_open );
		break;
	case PXENV_TFTP_CLOSE:
		ret = pxenv_tftp_close ( &params->tftp_close );
		break;
	case PXENV_TFTP_READ:
		ret = pxenv_tftp_read ( &params->tftp_read );
		break;
	case PXENV_TFTP_READ_FILE:
		ret = pxenv_tftp_read_file ( &params->tftp_read_file );
		break;
	case PXENV_TFTP_GET_FSIZE:
		ret = pxenv_tftp_get_fsize ( &params->tftp_get_fsize );
		break;
	case PXENV_UDP_OPEN:
		ret = pxenv_udp_open ( &params->udp_open );
		break;
	case PXENV_UDP_CLOSE:
		ret = pxenv_udp_close ( &params->udp_close );
		break;
	case PXENV_UDP_READ:
		ret = pxenv_udp_read ( &params->udp_read );
		break;
	case PXENV_UDP_WRITE:
		ret = pxenv_udp_write ( &params->udp_write );
		break;
	case PXENV_UNLOAD_STACK:
		ret = pxenv_unload_stack ( &params->unload_stack );
		break;
	case PXENV_GET_CACHED_INFO:
		ret = pxenv_get_cached_info ( &params->get_cached_info );
		break;
	case PXENV_RESTART_TFTP:
		ret = pxenv_restart_tftp ( &params->restart_tftp );
		break;
	case PXENV_START_BASE:
		ret = pxenv_start_base ( &params->start_base );
		break;
	case PXENV_STOP_BASE:
		ret = pxenv_stop_base ( &params->stop_base );
		break;
		
	default:
		DBG ( "PXENV_UNKNOWN_%hx", opcode );
		params->Status = PXENV_STATUS_UNSUPPORTED;
		ret = PXENV_EXIT_FAILURE;
		break;
	}

	if ( params->Status != PXENV_STATUS_SUCCESS ) {
		DBG ( " %hx", params->Status );
	}
	if ( ret != PXENV_EXIT_SUCCESS ) {
		DBG ( ret == PXENV_EXIT_FAILURE ? " err" : " ??" );
	}
	DBG ( "]" );

	return ret;
}

#endif /* PXE_EXPORT */
