/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include	"etherboot.h"
#include	"nic.h"

#undef	INCLUDE_PCI
#if	defined(INCLUDE_NS8390) || defined(INCLUDE_EEPRO100) || defined(INCLUDE_E1000) || defined(INCLUDE_LANCE) || defined(INCLUDE_EPIC100) || defined(INCLUDE_TULIP) || defined(INCLUDE_OTULIP) || defined(INCLUDE_3C90X) ||  defined(INCLUDE_3C595) || defined(INCLUDE_RTL8139) || defined(INCLUDE_VIA_RHINE) || defined(INCLUDE_W89C840) || defined(INCLUDE_DAVICOM) || defined(INCLUDE_SIS900) || defined(INCLUDE_NATSEMI) || defined(INCLUDE_FA311) || defined(INCLUDE_TLAN) || defined(INCLUDE_PRISM2_PLX) || defined(INCLUDE_PRISM2_PCI)
	/* || others later */
#define	INCLUDE_PCI
#include	"pci.h"

#define PCI_NIC(strname, probe, list) { strname, probe, list, sizeof(list)/sizeof(list[0]) }
#ifdef	INCLUDE_NS8390
static struct pci_id nepci_nics[] = {
	{ PCI_VENDOR_ID_REALTEK,	PCI_DEVICE_ID_REALTEK_8029,
		"Realtek 8029" },
	{ PCI_VENDOR_ID_DLINK,		0x0300,
		"DE-528" },
	{ PCI_VENDOR_ID_WINBOND2,	PCI_DEVICE_ID_WINBOND2_89C940,
		"Winbond NE2000-PCI" },
	{ PCI_VENDOR_ID_COMPEX,		PCI_DEVICE_ID_COMPEX_RL2000,
		"Compex ReadyLink 2000" },
	{ PCI_VENDOR_ID_KTI,		PCI_DEVICE_ID_KTI_ET32P2,
		"KTI ET32P2" },
	{ PCI_VENDOR_ID_NETVIN,		PCI_DEVICE_ID_NETVIN_NV5000SC,
		"NetVin NV5000SC" },
	{ PCI_VENDOR_ID_HOLTEK,		PCI_DEVICE_ID_HOLTEK_HT80232,
		"Holtek HT80232" },
};
#endif
#ifdef	INCLUDE_3C90X
static struct pci_id a3c90x_nics[] = {
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C900TPO,
		"3Com900-TPO" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C900COMBO,
		"3Com900-Combo" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C905TX,
		"3Com905-TX" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C905T4,
		"3Com905-T4" },

	{ PCI_VENDOR_ID_3COM,		0x9004,
		"3Com900B-TPO" },
	{ PCI_VENDOR_ID_3COM,		0x9005,
		"3Com900B-Combo" },
	{ PCI_VENDOR_ID_3COM,		0x9006,
		"3Com900B-2/T" },
	{ PCI_VENDOR_ID_3COM,		0x900A,
		"3Com900B-FL" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C905B_TX,
		"3Com905B-TX" },
	{ PCI_VENDOR_ID_3COM,		0x9056,
		"3Com905B-T4"},
	{ PCI_VENDOR_ID_3COM,		0x9058,
		"3Com905B-9058"},
	{ PCI_VENDOR_ID_3COM,		0x905A,
		"3Com905B-FL" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C905C_TXM,
		"3Com905C-TXM" },
	{ PCI_VENDOR_ID_3COM,		0x9800,
		"3Com980-Cyclone" },
	{ PCI_VENDOR_ID_3COM,		0x9805,
		"3Com9805" },
	{ PCI_VENDOR_ID_3COM,		0x7646,
		"3CSOHO100-TX" },
};
#endif
#ifdef	INCLUDE_3C595
static struct pci_id t595_nics[] = {
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C590,
		"3Com590" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C595,
		"3Com595" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C595_1,
		"3Com595" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C595_2,
		"3Com595" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C900TPO,
		"3Com900-TPO" },
	{ PCI_VENDOR_ID_3COM,		PCI_DEVICE_ID_3COM_3C900COMBO,
		"3Com900-Combo" },
	{ PCI_VENDOR_ID_3COM,		0x9004,
		"3Com900B-TPO" },
	{ PCI_VENDOR_ID_3COM,		0x9005,
		"3Com900B-Combo" },
	{ PCI_VENDOR_ID_3COM,		0x9006,
		"3Com900B-2/T" },
	{ PCI_VENDOR_ID_3COM,		0x900A,
		"3Com900B-FL" },
	{ PCI_VENDOR_ID_3COM,		0x9800,
		"3Com980-Cyclone" },
	{ PCI_VENDOR_ID_3COM,		0x9805,
		"3Com9805" },
	{ PCI_VENDOR_ID_3COM,		0x7646,
		"3CSOHO100-TX" },
};
#endif
#ifdef	INCLUDE_EEPRO100
static struct pci_id eepro100_nics[] = {
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_82557,
		"Intel EtherExpressPro100" },
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_82559ER,
		"Intel EtherExpressPro100 82559ER" },
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_ID1029,
		"Intel EtherExpressPro100 ID1029" },
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_ID1030,
		"Intel Corporation 82559 InBusiness 10/100" },
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_82562,
		"Intel EtherExpressPro100 82562EM" },
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_ID1038,
		"Intel(R) PRO/100 VM Network Connection" },
	{ PCI_VENDOR_ID_INTEL,		0x1039,
		"Intel PRO100 VE 82562ET" },
};
#endif
#ifdef INCLUDE_E1000
static struct pci_id e1000_nics[] = {
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_82542,
               "Intel EtherExpressPro1000" },
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_82543GC_FIBER,
               "Intel EtherExpressPro1000 82543GC Fiber" },
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_82543GC_COPPER,
               "Intel EtherExpressPro1000 82543GC Copper" },
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_82544EI_COPPER,
               "Intel EtherExpressPro1000 82544EI Copper" },
	{ PCI_VENDOR_ID_INTEL,		PCI_DEVICE_ID_INTEL_82544GC_CREB,
               "Intel EtherExpressPro1000 82544GC Creb" },
};
#endif

#ifdef	INCLUDE_EPIC100
static struct pci_id epic100_nics[] = {
	{ PCI_VENDOR_ID_SMC,		PCI_DEVICE_ID_SMC_EPIC100,
		"SMC EtherPowerII" },
};
#endif
#ifdef	INCLUDE_LANCE
static struct pci_id lance_nics[] = {
	{ PCI_VENDOR_ID_AMD,		PCI_DEVICE_ID_AMD_LANCE,
		"AMD Lance/PCI" },
	{ PCI_VENDOR_ID_AMD_HOMEPNA,	PCI_DEVICE_ID_AMD_HOMEPNA,
		"AMD Lance/HomePNA" },
};
#endif
#ifdef	INCLUDE_RTL8139
static struct pci_id rtl8139_nics[] = {
	{ PCI_VENDOR_ID_REALTEK,	PCI_DEVICE_ID_REALTEK_8129,
		"Realtek 8129" },
	{ PCI_VENDOR_ID_REALTEK,	PCI_DEVICE_ID_REALTEK_8139,
		"Realtek 8139" },
	{ PCI_VENDOR_ID_DLINK,		PCI_DEVICE_ID_DFE530TXP,
                "DFE530TX+/DFE538TX" },
        { PCI_VENDOR_ID_SMC_1211,       PCI_DEVICE_ID_SMC_1211,
                "SMC EZ10/100" },
};
#endif
#ifdef	INCLUDE_OTULIP
static struct pci_id otulip_nics[] = {
	{ PCI_VENDOR_ID_DEC,		PCI_DEVICE_ID_DEC_TULIP,
		"Digital Tulip" },
	{ PCI_VENDOR_ID_DEC,		PCI_DEVICE_ID_DEC_TULIP_FAST,
		"Digital Tulip Fast" },
	{ PCI_VENDOR_ID_DEC,		PCI_DEVICE_ID_DEC_TULIP_PLUS,
		"Digital Tulip+" },
	{ PCI_VENDOR_ID_DEC,		PCI_DEVICE_ID_DEC_21142,
		"Digital Tulip 21142" },
};
#endif
#ifdef	INCLUDE_TULIP
static struct pci_id tulip_nics[] = {
	{ PCI_VENDOR_ID_DEC,		PCI_DEVICE_ID_DEC_TULIP,
		"Digital Tulip" },
	{ PCI_VENDOR_ID_DEC,		PCI_DEVICE_ID_DEC_TULIP_FAST,
		"Digital Tulip Fast" },
	{ PCI_VENDOR_ID_DEC,		PCI_DEVICE_ID_DEC_TULIP_PLUS,
		"Digital Tulip+" },
	{ PCI_VENDOR_ID_DEC,		PCI_DEVICE_ID_DEC_21142,
		"Digital Tulip 21142" },
	{ PCI_VENDOR_ID_MACRONIX,	PCI_DEVICE_ID_MX987x3,
		"Macronix MX987x3" },
	{ PCI_VENDOR_ID_MACRONIX,	PCI_DEVICE_ID_MX987x5,
		"Macronix MX987x5" },
	{ PCI_VENDOR_ID_LINKSYS,	PCI_DEVICE_ID_LC82C115,
		"LinkSys LNE100TX" },
	{ PCI_VENDOR_ID_LINKSYS,	PCI_DEVICE_ID_DEC_TULIP,
		"Netgear FA310TX" },
	{ PCI_VENDOR_ID_DAVICOM, PCI_DEVICE_ID_DM9102,
		"Davicom 9102" },
	{ PCI_VENDOR_ID_DAVICOM, PCI_DEVICE_ID_DM9009,
		"Davicom 9009" },
	{ PCI_VENDOR_ID_ADMTEK, PCI_DEVICE_ID_ADMTEK_0985,
		"ADMtek Centaur-P" },
	{ PCI_VENDOR_ID_ADMTEK, 0x0981,
		"ADMtek AN981 Comet" },
	{ PCI_VENDOR_ID_SMC_1211, 0x1216,
		"ADMTek AN983 Comet" },
        { 0x125B, 0x1400,
		"ASIX AX88140"},
        { 0x11F6, 0x9881,
		"Compex RL100-TX"},
};
#endif
#ifdef	INCLUDE_DAVICOM
static struct pci_id davicom_nics[] = {
	{ PCI_VENDOR_ID_DAVICOM, PCI_DEVICE_ID_DM9102,
		"Davicom 9102" },
	{ PCI_VENDOR_ID_DAVICOM, PCI_DEVICE_ID_DM9009,
		"Davicom 9009" },
};
#endif
#ifdef	INCLUDE_VIA_RHINE
static struct pci_id rhine_nics[] = {
	{ PCI_VENDOR_ID_VIATEC, PCI_DEVICE_ID_VIA_VT6102,
		"VIA 6102" },
	{ PCI_VENDOR_ID_VIATEC,	PCI_DEVICE_ID_VIA_RHINE_I,
		"VIA 3043" },
	{ PCI_VENDOR_ID_VIATEC,	PCI_DEVICE_ID_VIA_86C100A,
		"VIA 86C100A" },
};
#endif
#ifdef	INCLUDE_W89C840
static struct pci_id w89c840_nics[] = {
	{ PCI_VENDOR_ID_WINBOND2,	PCI_DEVICE_ID_WINBOND2_89C840,
		"Winbond W89C840F" },
	{ PCI_VENDOR_ID_COMPEX,	PCI_DEVICE_ID_COMPEX_RL100ATX,
		"Compex RL100ATX" },
};
#endif
#ifdef INCLUDE_SIS900
static struct pci_id sis900_nics[] = {
       { PCI_VENDOR_ID_SIS,     	PCI_DEVICE_ID_SIS900,
         "SIS900" },
       { PCI_VENDOR_ID_SIS,     	PCI_DEVICE_ID_SIS7016,
	 "SIS7016" },
};
#endif

#ifdef INCLUDE_NATSEMI
static struct pci_id natsemi_nics[] = {
       { PCI_VENDOR_ID_NS,	     	PCI_DEVICE_ID_DP83815,
         "DP83815" },
};
#endif

#ifdef INCLUDE_FA311
static struct pci_id fa311_nics[] = {
       { PCI_VENDOR_ID_NS,	     	PCI_DEVICE_ID_DP83815,
         "DP83815" },
};
#endif

#ifdef	INCLUDE_TLAN
static struct pci_id tlan_nics[] = {
	{ PCI_VENDOR_ID_OLICOM,		PCI_DEVICE_ID_OLICOM_OC2326,
	  "OC2326" },
};
#endif

#ifdef	INCLUDE_PRISM2_PLX
static struct pci_id prism2_plx_nics[] = {
	{ PCI_VENDOR_ID_NETGEAR,	PCI_DEVICE_ID_NETGEAR_MA301,
	  "Netgear MA301" },
};
#endif

#ifdef	INCLUDE_PRISM2_PCI
static struct pci_id prism2_pci_nics[] = {
	{ PCI_VENDOR_ID_HARRIS,		PCI_DEVICE_ID_HARRIS_PRISM2,
	  "Harris Semiconductor Prism2.5 clone" },
};
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
	PCI_NIC("3C595", t595_probe, t595_nics),
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
	PCI_NIC( "NE2000/PCI", nepci_probe, nepci_nics),
#endif
#ifdef	INCLUDE_LANCE
	PCI_NIC( "LANCE/PCI", lancepci_probe, lance_nics),
#endif
#ifdef	INCLUDE_VIA_RHINE
	PCI_NIC( "VIA 86C100", rhine_probe, rhine_nics),
#endif
#ifdef	INCLUDE_W89C840
	PCI_NIC( "W89C840F", w89c840_probe, w89c840_nics),
#endif
#ifdef	INCLUDE_TLAN
	PCI_NIC( "Olicom 2326", tlan_probe, tlan_nics),
#endif
#ifdef	INCLUDE_PRISM2_PLX
	PCI_NIC( "Prism2_PLX", prism2_plx_probe, prism2_plx_nics),
#endif
#ifdef	INCLUDE_PRISM2_PCI
	PCI_NIC( "Prism2_PCI", prism2_pci_probe, prism2_pci_nics),
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
