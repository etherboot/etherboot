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
#include "nic.h"

#if TRACE_PXE
#define DBG(...) printf ( __VA_ARGS__ )
#else
#define DBG(...)
#endif

/* Global pointer to currently installed PXE stack */
pxe_stack_t *pxe_stack = NULL;

/* PXENV_START_UNDI
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_start_undi ( t_PXENV_START_UNDI *start_undi ) {
	DBG ( "PXENV_START_UNDI" );
	start_undi->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_STARTUP
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_startup ( t_PXENV_UNDI_STARTUP *undi_startup ) {
	DBG ( "PXENV_UNDI_STARTUP" );
	undi_startup->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_CLEANUP
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_cleanup ( t_PXENV_UNDI_CLEANUP *undi_cleanup ) {
	DBG ( "PXENV_UNDI_CLEANUP" );
	undi_cleanup->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_INITIALIZE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_initialize ( t_PXENV_UNDI_INITIALIZE
				     *undi_initialize ) {
	DBG ( "PXENV_UNDI_INITIALIZE" );
	undi_initialize->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_RESET_ADAPTER
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_reset_adapter ( t_PXENV_UNDI_RESET_ADAPTER
					*undi_reset_adapter ) {
	DBG ( "PXENV_UNDI_RESET_ADAPTER" );
	undi_reset_adapter->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_SHUTDOWN
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_shutdown ( t_PXENV_UNDI_SHUTDOWN *undi_shutdown ) {
	DBG ( "PXENV_UNDI_SHUTDOWN" );
	undi_shutdown->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_OPEN
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_open ( t_PXENV_UNDI_OPEN *undi_open ) {
	DBG ( "PXENV_UNDI_OPEN" );
	undi_open->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_CLOSE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_close ( t_PXENV_UNDI_CLOSE *undi_close ) {
	DBG ( "PXENV_UNDI_CLOSE" );
	undi_close->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_TRANSMIT
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_transmit ( t_PXENV_UNDI_TRANSMIT *undi_transmit ) {
	DBG ( "PXENV_UNDI_TRANSMIT" );
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
	undi_set_mcast_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_SET_STATION_ADDRESS
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_set_station_address ( t_PXENV_UNDI_SET_STATION_ADDRESS
					      *undi_set_station_address ) {
	DBG ( "PXENV_UNDI_SET_STATION_ADDRESS" );
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
	undi_set_packet_filter->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_INFORMATION
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_get_information ( t_PXENV_UNDI_GET_INFORMATION
					  *undi_get_information ) {
	DBG ( "PXENV_UNDI_GET_INFORMATION" );
	undi_get_information->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_STATISTICS
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_get_statistics ( t_PXENV_UNDI_GET_STATISTICS
					 *undi_get_statistics ) {
	DBG ( "PXENV_UNDI_GET_STATISTICS" );
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
	undi_get_mcast_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_NIC_TYPE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_get_nic_type ( t_PXENV_UNDI_GET_NIC_TYPE
				       *undi_get_nic_type ) {
	DBG ( "PXENV_UNDI_GET_NIC_TYPE" );
	undi_get_nic_type->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_IFACE_INFO
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_get_iface_info ( t_PXENV_UNDI_GET_IFACE_INFO
					 *undi_get_iface_info ) {
	DBG ( "PXENV_UNDI_GET_IFACE_INFO" );
	undi_get_iface_info->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_ISR
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_undi_isr ( t_PXENV_UNDI_ISR *undi_isr ) {
	DBG ( "PXENV_UNDI_ISR" );
	undi_isr->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_STOP_UNDI
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_stop_undi ( t_PXENV_STOP_UNDI *stop_undi ) {
	DBG ( "PXENV_STOP_UNDI" );
	stop_undi->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_TFTP_OPEN
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_open ( t_PXENV_TFTP_OPEN *tftp_open ) {
	DBG ( "PXENV_TFTP_OPEN" );
	tftp_open->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_TFTP_CLOSE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_close ( t_PXENV_TFTP_CLOSE *tftp_close ) {
	DBG ( "PXENV_TFTP_CLOSE" );
	tftp_close->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_TFTP_READ
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_read ( t_PXENV_TFTP_READ *tftp_read ) {
	DBG ( "PXENV_TFTP_READ" );
	tftp_read->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_TFTP_READ_FILE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_read_file ( t_PXENV_TFTP_READ_FILE *tftp_read_file ) {
	DBG ( "PXENV_TFTP_READ_FILE" );
	tftp_read_file->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_TFTP_GET_FSIZE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_tftp_get_fsize ( t_PXENV_TFTP_GET_FSIZE *tftp_get_fsize ) {
	DBG ( "PXENV_TFTP_GET_FSIZE" );
	tftp_get_fsize->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UDP_OPEN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_udp_open ( t_PXENV_UDP_OPEN *udp_open ) {
	DBG ( "PXENV_UDP_OPEN" );

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
	udp_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UDP_READ
 *
 * Status: working
 */
int await_pxe_udp ( int ival __unused, void *ptr,
		    unsigned short ptype __unused,
		    struct iphdr *ip, struct udphdr *udp ) {
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
 * Status: stub
 */
PXENV_EXIT_t pxenv_unload_stack ( t_PXENV_UNLOAD_STACK *unload_stack ) {
	DBG ( "PXENV_UNLOAD_STACK" );
	unload_stack->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_GET_CACHED_INFO
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_get_cached_info ( t_PXENV_GET_CACHED_INFO
				     *get_cached_info ) {
	BOOTPLAYER *cached_info = &pxe_stack->cached_info;
	DBG ( "PXENV_GET_CACHED_INFO" );

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
	/* FIXME: pretend not to have any vendor DHCP options */
	memset ( &cached_info->vendor, 0, sizeof(cached_info->vendor) );

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
		if ( get_cached_info->BufferSize < size )
			size = get_cached_info->BufferSize;
		memcpy ( SEGOFF16_TO_PTR ( get_cached_info->Buffer ),
			 cached_info, size );
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
	restart_tftp->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_START_BASE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_start_base ( t_PXENV_START_BASE *start_base ) {
	DBG ( "PXENV_START_BASE" );
	start_base->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_STOP_BASE
 *
 * Status: stub
 */
PXENV_EXIT_t pxenv_stop_base ( t_PXENV_STOP_BASE *stop_base ) {
	DBG ( "PXENV_STOP_BASE" );
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
