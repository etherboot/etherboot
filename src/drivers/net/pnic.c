/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Bochs Pseudo NIC driver for Etherboot
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * See pnic_api.h for an explanation of the Bochs Pseudo NIC.
 */

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the PCI support functions, if this is a PCI NIC */
#include "pci.h"

/* PNIC API */
#include "pnic_api.h"

/* NIC specific static variables go here */
static uint16_t ioaddr;
static uint8_t rx_tx_buffer[ETH_FRAME_LEN];
uint8_t *rx_buffer = rx_tx_buffer;
uint8_t *tx_buffer = rx_tx_buffer;

/* 
 * Utility functions: issue a PNIC command, retrieve result.  Use
 * pnic_command_quiet if you don't want failure codes to be
 * automatically printed.  Returns the PNIC status code.
 * 
 * Set output_length to NULL only if you expect to receive exactly
 * output_max_length bytes, otherwise it'll complain that you didn't
 * get enough data (on the assumption that if you not interested in
 * discovering the output length then you're expecting a fixed amount
 * of data).
 */

uint16_t pnic_command_quiet ( uint16_t command,
			      void *input, uint16_t input_length,
			      void *output, uint16_t output_max_length,
			      uint16_t *output_length ) {
	int i;
	uint16_t status;
	uint16_t _output_length;

	if ( input != NULL ) {
		/* Write input length */
		outw ( input_length, ioaddr + PNIC_REG_LEN );
		/* Write input data */
		for ( i = 0; i < input_length; i++ ) {
			outb ( ((char*)input)[i], ioaddr + PNIC_REG_DATA );
		}
	}
	/* Write command */
	outw ( command, ioaddr + PNIC_REG_CMD );
	/* Retrieve status */
	status = inw ( ioaddr + PNIC_REG_STAT );
	/* Retrieve output length */
	_output_length = inw ( ioaddr + PNIC_REG_LEN );
	if ( output_length == NULL ) {
		if ( _output_length != output_max_length ) {
			printf ( "pnic_command %#hx: wrong data length "
				 "returned (expected %d, got %d)\n", command,
				 output_max_length, _output_length );
		}
	} else {
		*output_length = _output_length;
	}
	if ( output != NULL ) {
		if ( _output_length > output_max_length ) {
			printf ( "pnic_command %#hx: output buffer too small "
				 "(have %d, need %d)\n", command,
				 output_max_length, _output_length );
			_output_length = output_max_length;
		}
		/* Retrieve output data */
		for ( i = 0; i < _output_length; i++ ) {
			((char*)output)[i] = inb ( ioaddr + PNIC_REG_DATA );
		}
	}
	return status;
}

uint16_t pnic_command ( uint16_t command,
			void *input, uint16_t input_length,
			void *output, uint16_t output_max_length,
			uint16_t *output_length ) {
	uint16_t status = pnic_command_quiet ( command, input, input_length,
					       output, output_max_length,
					       output_length );
	if ( status == PNIC_STATUS_OK ) return status;
	printf ( "PNIC command %#hx (len %#hx) failed with status %#hx\n",
		 command, input_length, status );
	return status;
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int pnic_poll(struct nic *nic)
{
	uint16_t length;

	if ( pnic_command ( PNIC_CMD_RECV, NULL, 0,
			    nic->packet, ETH_FRAME_LEN, &length )
	     != PNIC_STATUS_OK ) return ( 0 );

	/* Length 0 => no packet */
	if ( length == 0 ) return ( 0 );

	nic->packetlen = length;
	return ( 1 );
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void pnic_transmit(
	struct nic *nic,
	const char *dest,		/* Destination */
	unsigned int type,		/* Type */
	unsigned int size,		/* size */
	const char *data)		/* Packet */
{
	unsigned int nstype = htons ( type );

	if ( ( ETH_HLEN + size ) >= ETH_FRAME_LEN ) {
		printf ( "pnic_transmit: packet too large\n" );
		return;
	}

	/* Assemble packet */
	memcpy ( tx_buffer, dest, ETH_ALEN );
	memcpy ( tx_buffer + ETH_ALEN, nic->node_addr, ETH_ALEN );
	memcpy ( tx_buffer + 2 * ETH_ALEN, &nstype, 2 );
	memcpy ( tx_buffer + ETH_HLEN, data, size );

	pnic_command ( PNIC_CMD_XMIT, tx_buffer, ETH_HLEN + size,
		       NULL, 0, NULL );
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void pnic_disable(struct dev *dev __unused)
{
	pnic_command ( PNIC_CMD_RESET, NULL, 0, NULL, 0, NULL );
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/

static int pnic_probe(struct dev *dev, struct pci_device *pci)
{
	struct nic *nic = (struct nic *)dev;
	uint16_t status;
	uint16_t api_version;

	printf(" - ");

	/* Mask the bit that says "this is an io addr" */
	ioaddr = pci->ioaddr & ~3;
	/* Not sure what this does, but the rtl8139 driver does it */
	adjust_pci_device(pci);

	status = pnic_command_quiet( PNIC_CMD_API_VER, NULL, 0,
				     &api_version, sizeof(api_version), NULL );
	if ( status != PNIC_STATUS_OK ) {
		printf ( "PNIC failed installation check, code %#hx\n",
			 status );
		return 0;
	}
	status = pnic_command ( PNIC_CMD_READ_MAC, NULL, 0,
				nic->node_addr, ETH_ALEN, NULL );
	printf ( "Detected Bochs Pseudo NIC MAC %! (API v%d.%d) at %#hx\n",
		 nic->node_addr, api_version>>8, api_version&0xff, ioaddr );
	if ( api_version != PNIC_API_VERSION ) {
		printf ( "Warning: API version mismatch! "
			 "(NIC's is %d.%d, ours is %d.%d)\n",
			 api_version >> 8, api_version & 0xff,
			 PNIC_API_VERSION >> 8, PNIC_API_VERSION & 0xff );
	}

	/* point to NIC specific routines */
	dev->disable  = pnic_disable;
	nic->poll     = pnic_poll;
	nic->transmit = pnic_transmit;
	return 1;
}

static struct pci_id pnic_nics[] = {
/* genrules.pl doesn't let us use macros for PCI IDs...*/
PCI_ROM(0xfefe, 0xefef, "pnic", "Bochs Pseudo NIC Adaptor"),
};

static struct pci_driver pnic_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "PNIC",
	.probe    = pnic_probe,
	.ids      = pnic_nics,
	.id_count = sizeof(pnic_nics)/sizeof(pnic_nics[0]),
	.class    = 0,
};
