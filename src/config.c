/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include	"etherboot.h"
#include	"nic.h"

#undef	INCLUDE_PCI
#if	defined(INCLUDE_NS8390) || defined(INCLUDE_EEPRO100) || defined(INCLUDE_E1000) || defined(INCLUDE_LANCE) || defined(INCLUDE_EPIC100) || defined(INCLUDE_TULIP) || defined(INCLUDE_OTULIP) || defined(INCLUDE_3C90X) ||  defined(INCLUDE_3C595) || defined(INCLUDE_RTL8139) || defined(INCLUDE_VIA_RHINE) || defined(INCLUDE_W89C840) || defined(INCLUDE_DAVICOM) || defined(INCLUDE_SIS900) || defined(INCLUDE_NATSEMI) || defined(INCLUDE_FA311) || defined(INCLUDE_TLAN) || defined(INCLUDE_PRISM2_PLX) || defined(INCLUDE_PRISM2_PCI) || defined(INCLUDE_SUNDANCE) || defined(INCLUDE_PCNET32) || defined(INCLUDE_TLAN)
	/* || others later */
#define	INCLUDE_PCI
#include	"pci.h"

#define PCI_NIC(strname, probe, list) { strname, probe, list, sizeof(list)/sizeof(list[0]) }
#ifdef	INCLUDE_NS8390
#include "ns8390_ids.h"
#endif
#ifdef	INCLUDE_3C90X
#include "3c90x_ids.h"
#endif
#ifdef	INCLUDE_3C595
#include "3c595_ids.h"
#endif
#ifdef	INCLUDE_EEPRO100
#include "eepro100_ids.h"
#endif
#ifdef INCLUDE_E1000
#include "e1000_ids.h"
#endif
#ifdef	INCLUDE_EPIC100
#include "epic100_ids.h"
#endif
#ifdef	INCLUDE_LANCE
#include "lance_ids.h"
#endif
#ifdef	INCLUDE_RTL8139
#include "rtl8139_ids.h"
#endif
#ifdef	INCLUDE_TULIP
#include "tulip_ids.h"
#endif
#ifdef	INCLUDE_DAVICOM
#include "davicom_ids.h"
#endif
#ifdef	INCLUDE_VIA_RHINE
#include "via-rhine_ids.h"
#endif
#ifdef	INCLUDE_W89C840
#include "w89c840_ids.h"
#endif
#ifdef INCLUDE_SIS900
#include "sis900_ids.h"
#endif
#ifdef INCLUDE_NATSEMI
#include "natsemi_ids.h"
#endif
#ifdef INCLUDE_FA311
#include "fa311_ids.h"
#endif
#ifdef	INCLUDE_TLAN
#include "tlan_ids.h"
#endif
#ifdef	INCLUDE_PRISM2_PLX
#include "prism2_plx_ids.h"
#endif
#ifdef	INCLUDE_PRISM2_PCI
#include "prism2_pci_ids.h"
#endif
#ifdef	INCLUDE_SUNDANCE
#include "sundance_ids.h"
#endif
#ifdef INCLUDE_PCNET32
#include "pcnet32_ids.h"
#endif
#ifdef INCLUDE_TLAN
#include "tlan_ids.h"
#endif
/* other PCI NICs go here */
#endif	/* INCLUDE_*PCI */

#include "cards.h"


struct dispatch_table
{
	const char	*nic_name;
#ifdef	INCLUDE_PCI
	struct nic	*(*eth_probe)(struct nic *, unsigned short *,
		struct pci_device *);
	struct pci_id *pci_ids;
	int pci_id_count;
#else
	struct nic	*(*eth_probe)(struct nic *, unsigned short *);
	unsigned short	*probe_ioaddrs;		/* for probe overrides */
#endif	/* INCLUDE_PCI */
};

/*
 *	NIC probing is in order of appearance in this table.
 *	If for some reason you want to change the order,
 *	just rearrange the entries (bracketed by the #ifdef/#endif)
 */
static const struct dispatch_table	NIC[] =
{
#ifdef	INCLUDE_RTL8139
	PCI_NIC( "RTL8139", rtl8139_probe, rtl8139_nics ),
#endif
#ifdef INCLUDE_SIS900
	PCI_NIC( "SIS900", sis900_probe, sis900_nics ),	
#endif
#ifdef INCLUDE_NATSEMI
	PCI_NIC( "NATSEMI", natsemi_probe, natsemi_nics ),
#endif
#ifdef INCLUDE_FA311
	PCI_NIC("FA311", fa311_probe, fa311_nics),
#endif
#ifdef	INCLUDE_WD
	{ "WD", wd_probe, 0 },
#endif
#ifdef	INCLUDE_3C503
	{ "3C503", t503_probe, 0 },
#endif
#ifdef	INCLUDE_NE
	{ "NE*000", ne_probe, 0 },
#endif
#ifdef	INCLUDE_3C509
	{ "3C5x9", t509_probe, 0 },
#endif
#ifdef	INCLUDE_3C529
	{ "3C5x9", t529_probe, 0 },
#endif
#ifdef  INCLUDE_3C515
	{ "3C515", t515_probe, 0 },
#endif
#ifdef	INCLUDE_3C595
	PCI_NIC("3C595", t595_probe, a3c595_nics),
#endif
#ifdef	INCLUDE_3C90X
	PCI_NIC("3C90X", a3c90x_probe, a3c90x_nics),
#endif
#ifdef	INCLUDE_EEPRO
	{ "EEPRO", eepro_probe, 0 },
#endif
#ifdef	INCLUDE_EEPRO100
	PCI_NIC( "EEPRO100", eepro100_probe, eepro100_nics),
#endif
#ifdef INCLUDE_E1000
	PCI_NIC( "E1000", e1000_probe, e1000_nics),
#endif

#ifdef	INCLUDE_EPIC100
	PCI_NIC( "EPIC100", epic100_probe, epic100_nics),
#endif
#ifdef	INCLUDE_OTULIP
	PCI_NIC( "OTulip", otulip_probe, otulip_nics),
#endif
#ifdef	INCLUDE_TULIP
	PCI_NIC( "Tulip", tulip_probe, tulip_nics),
#endif
#ifdef	INCLUDE_DAVICOM
	PCI_NIC( "DAVICOM", davicom_probe, davicom_nics),
#endif
#ifdef	INCLUDE_CS89X0
	{ "CS89x0", cs89x0_probe, 0 },
#endif
#ifdef	INCLUDE_NE2100
	{ "NE2100", ne2100_probe, 0 },
#endif
#ifdef	INCLUDE_NI6510
	{ "NI6510", ni6510_probe, 0 },
#endif
#ifdef	INCLUDE_SK_G16
	{ "SK_G16", SK_probe, 0 },
#endif
#ifdef	INCLUDE_3C507
	{ "3C507", t507_probe, 0 },
#endif
#ifdef	INCLUDE_NI5010
	{ "NI5010", ni5010_probe, 0 },
#endif
#ifdef	INCLUDE_NI5210
	{ "NI5210", ni5210_probe, 0 },
#endif
#ifdef	INCLUDE_EXOS205
	{ "EXOS205", exos205_probe, 0 },
#endif
#ifdef	INCLUDE_SMC9000
	{ "SMC9000", smc9000_probe, 0 },
#endif
#ifdef	INCLUDE_TIARA
	{ "TIARA", tiara_probe, 0 },
#endif
#ifdef	INCLUDE_DEPCA
	{ "DEPCA", depca_probe, 0 },
#endif
#ifdef	INCLUDE_NS8390
	PCI_NIC( "NE2000/PCI", nepci_probe, ns8390_nics),
#endif
#ifdef	INCLUDE_LANCE
	PCI_NIC( "LANCE/PCI", lancepci_probe, lance_nics),
#endif
#ifdef	INCLUDE_VIA_RHINE
	PCI_NIC( "VIA 86C100", rhine_probe, via_rhine_nics),
#endif
#ifdef	INCLUDE_W89C840
	PCI_NIC( "W89C840F", w89c840_probe, w89c840_nics),
#endif
#ifdef	INCLUDE_TLAN
	PCI_NIC( "Thunder Lan", tlan_probe, tlan_nics),
#endif
#ifdef	INCLUDE_PRISM2_PLX
	PCI_NIC( "Prism2_PLX", prism2_plx_probe, prism2_plx_nics),
#endif
#ifdef	INCLUDE_PRISM2_PCI
	PCI_NIC( "Prism2_PCI", prism2_pci_probe, prism2_pci_nics),
#endif
#ifdef	INCLUDE_SUNDANCE
	PCI_NIC( "Sundance", sundance_probe, sundance_nics),
#endif
#ifdef INCLUDE_PCNET32
	PCI_NIC( "PCNet32", pcnet32_probe, pcnet32_nics),
#endif
		
	/* this entry must always be last to mark the end of list */
#ifdef INCLUDE_PCI
	{ 0, 0, 0, 0 }
#else
	{ 0, 0, 0 }
#endif
};

#define	NIC_TABLE_SIZE	(sizeof(NIC)/sizeof(NIC[0]))

static int eth_dummy(struct nic *nic)
{
	return (0);
}

static char	packet[ETH_FRAME_LEN];

struct nic	nic =
{
	(void (*)(struct nic *))eth_dummy,	/* reset */
	eth_dummy,				/* poll */
	(void (*)(struct nic *, const char *,
		unsigned int, unsigned int,
		const char *))eth_dummy,		/* transmit */
	(void (*)(struct nic *))eth_dummy,	/* disable */
#ifdef	T503_AUI
	1,			/* aui */
#else
	0,			/* no aui */
#endif
	&rom,			/* rom_info */
	arptable[ARP_CLIENT].node,	/* node_addr */
	packet,			/* packet */
	0,			/* packetlen */
	{
		NIC_ID_SIZE-1,
		RFC1533_VENDOR_NIC_DEV_ID,
		5,
		PCI_BUS_TYPE,
		0,
		0
	},	/* devid */
	0,			/* priv_data */
};

void print_config(void)
{
	const struct dispatch_table	*t;

	printf("Etherboot " VERSION " (GPL) "
#ifdef	TAGGED_IMAGE
		"Tagged "
#endif
#ifdef	ELF_IMAGE
		"ELF "
#endif
#ifdef	IMAGE_FREEBSD
		"(FreeBSD) "
#endif
#ifdef	IMAGE_NETBSD
		"(NetBSD) "
#endif
#ifdef	IMAGE_MULTIBOOT
		"(Multiboot) "
#endif
#ifdef	AOUT_IMAGE
		"a.out "
#endif
		"for ");
	for (t = NIC; t->nic_name != 0; ++t)
		printf("[%s]", t->nic_name);
	putchar('\n');
}

void eth_reset(void)
{
	(*nic.reset)(&nic);
}


int eth_probe(int last_adapter)
{
#ifdef INCLUDE_PCI
	static unsigned short	pci_ioaddrs[2];
	static struct pci_device dev;
#endif
	static const struct dispatch_table *t = NIC;

#ifndef TRY_ALL_DEVICES
	/* Force a reset of the list every time... */
	last_adapter = -1;
#endif
	if (last_adapter == -1) {
		t = NIC;
#ifdef	INCLUDE_PCI
		dev.probe_id = 0;
		dev.bus = 0;
		dev.devfn = 0;
#endif
	}
	printf("Probing...");
	for (t = NIC; t->nic_name != 0; ++t)
	{
		printf("[%s]", t->nic_name);
#ifdef	INCLUDE_PCI
		eth_find_pci(t->pci_ids, t->pci_id_count, &dev);
		if (dev.probe_id == 0) {
			continue;
		}
#if 0
		if (dev.romaddr != ((unsigned long) rom.rom_segment << 4)) {
			continue;
		}
#endif
		pci_ioaddrs[0] = dev.ioaddr;
		pci_ioaddrs[1] = 0;
		nic.devid.bus_type = PCI_BUS_TYPE;
		nic.devid.vendor_id = htons(dev.vendor);
		nic.devid.device_id = htons(dev.dev_id);
		if ((*t->eth_probe)(&nic, pci_ioaddrs, &dev))
			return (0);
#else
		nic.devid.bus_type = ISA_BUS_TYPE;
		/* driver will fill in vendor and device IDs */
		if ((*t->eth_probe)(&nic, t->probe_ioaddrs))
			return (0);
#endif	/* INCLUDE_PCI */
	}
	return (-1);
}

int eth_poll(void)
{
	return ((*nic.poll)(&nic));
}

void eth_transmit(const char *d, unsigned int t, unsigned int s, const void *p)
{
	(*nic.transmit)(&nic, d, t, s, p);
	if (t == IP) twiddle();
}

void eth_disable(void)
{
	(*nic.disable)(&nic);
}
