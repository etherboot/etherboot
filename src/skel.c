/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Skeleton NIC driver for Etherboot
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
/* to get our own prototype */
#include "cards.h"

/* NIC specific static variables go here */

/**************************************************************************
RESET - Reset adapter
***************************************************************************/
static void skel_reset(struct nic *nic)
{
	/* put the card in its initial state */
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int skel_poll(struct nic *nic)
{
	/* return true if there's an ethernet packet ready to read */
	/* nic->packet should contain data on return */
	/* nic->packetlen should contain length of data */
	return (0);	/* initially as this is called to flush the input */
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void skel_transmit(
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
static void skel_disable(struct nic *nic)
{
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
You should omit the last argument struct pci_device * for a non-PCI NIC
***************************************************************************/
struct nic *skel_probe(struct nic *nic, unsigned short *probe_addrs,
	struct pci_device *p)
{
	/* if probe_addrs is 0, then routine can use a hardwired default */
	/* if board found */
	{
		/* point to NIC specific routines */
		nic->reset = skel_reset;
		nic->poll = skel_poll;
		nic->transmit = skel_transmit;
		nic->disable = skel_disable;
		return nic;
	}
	/* else */
	return 0;
}
