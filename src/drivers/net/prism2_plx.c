/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Prism2 NIC driver for Etherboot
Wrapper for prism2_plx

Written by Michael Brown of Fen Systems Ltd
$Id$
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#define WLAN_HOSTIF WLAN_PLX
#include "prism2.c"

static struct pci_id prism2_plx_nics[] = {
PCI_ROM(0x1385, 0x4100, "ma301", "Netgear MA301"),
};

static struct pci_driver prism2_plx_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "Prism2_PLX",
	.probe    = prism2_plx_probe,
	.ids      = prism2_plx_nics,
	.id_count = sizeof(prism2_plx_nics)/sizeof(prism2_plx_nics[0]),
	.class    = 0,
};

