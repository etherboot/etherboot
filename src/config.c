/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include	"etherboot.h"
#include	"nic.h"
#ifdef CONFIG_PCI
#include	"pci.h"
#endif
#ifdef CONFIG_ISA
#include	"isa.h"
#endif

static int dummy(void *unused __unused)
{
	return (0);
}

static char	packet[ETH_FRAME_LEN];

struct nic	nic =
{
	{
		(void (*)(struct dev *))dummy,	/* dev.disable */
		
		{
			DEV_ID_SIZE-1,
			RFC1533_VENDOR_NIC_DEV_ID,
			5,
			PCI_BUS_TYPE,
			0,
			0
		},				/* dev.devid */
	},
	(int (*)(struct nic *))dummy,		/* poll */
	(void (*)(struct nic *, const char *,
		unsigned int, unsigned int,
		const char *))dummy,		/* transmit */
	0,					/* flags */
	&rom,					/* rom_info */
	arptable[ARP_CLIENT].node,		/* node_addr */
	packet,					/* packet */
	0,					/* packetlen */
	0,					/* priv_data */
};

void print_config(void)
{
	printf("Etherboot " VERSION " (GPL) "
#ifdef	TAGGED_IMAGE
		"Tagged "
#endif
#ifdef	ELF64_IMAGE
		"ELF64 "
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
#ifdef	WINCE_IMAGE
		"WINCE "
#endif
		"for ");

#ifdef CONFIG_PCI
{
	const struct pci_driver *driver;
	for(driver = pci_drivers; driver < epci_drivers; driver++) {
		printf("[%s]", driver->name);
	}
}
#endif	
#ifdef CONFIG_ISA
{
	const struct isa_driver *driver;
	for(driver = isa_drivers; driver < eisa_drivers; driver++) {
		printf("[%s]", driver->name);
	}
}
#endif	
	putchar('\n');
}

#ifdef CONFIG_PCI
int pci_probe(int last_adapter, struct dev *dev, int type)
{
	static struct pci_device pdev;
	const struct pci_driver *driver;
	if (last_adapter == -1) {
		pdev.probe_id = 0;
		pdev.bus = 0;
		pdev.devfn = 0;
	}
	for(driver = pci_drivers; driver < epci_drivers; driver++) {
		if (driver->type != type)
			continue;
		printf("[%s]", driver->name);
		eth_find_pci(driver->ids, driver->id_count, &pdev);
		if (pdev.probe_id == 0) 
			continue;
#if 0
		if (pdev.romaddr != ((unsigned long) rom.rom_segment << 4)) {
			continue;
		}
#endif
		dev->devid.bus_type = PCI_BUS_TYPE;
		dev->devid.vendor_id = htons(pdev.vendor);
		dev->devid.device_id = htons(pdev.dev_id);
		
		if ((*driver->probe)(dev, &pdev))
			return 0;
	}
	return -1;
}
#else
#define pci_probe(x, y) (-1)
#endif

#ifdef CONFIG_ISA
int isa_probe(int last_adapter, struct dev *dev, int type)
{
/*
 *	NIC probing is in link order.
 *	If for some reason you want to change the order,
 *	just change the order you list the drivers in.
 */
	static const struct isa_driver *driver;
	if (last_adapter == -1) {
		driver = isa_drivers;
	}
	for(; driver < eisa_drivers; driver++)
	{
		if (driver->type != type)
			continue;
		printf("[%s]", driver->name);
		dev->devid.bus_type = ISA_BUS_TYPE;
		/* driver will fill in vendor and device IDs */
		if ((*driver->probe)(dev, driver->ioaddrs))
			return 0;
	}
	return -1;
}
#else
#define isa_probe(x, y) (-1)
#endif

int probe(int last_adapter, struct dev *dev, int type)
{
	static int to_probe;
#define PROBE_NONE 0
#define PROBE_PCI  1
#define PROBE_ISA  2
	int adapter;
	
	printf("Probing...");
	adapter = -1;
#ifndef TRY_ALL_DEVICES
	last_adapter = -1;
#endif
	if (last_adapter == -1) {
		to_probe = PROBE_PCI;
	}
	if (to_probe == PROBE_PCI) {
		adapter = pci_probe(last_adapter, dev, type);
		if (adapter == -1) {
			to_probe = PROBE_ISA;
		}
	}
	if (to_probe == PROBE_ISA) {
		adapter = isa_probe(last_adapter, dev, type);
		if (adapter == -1) {
			to_probe = PROBE_NONE;
		}
	}
	return adapter;
}

int eth_probe(int last_adapter)
{
	return probe(last_adapter, &nic.dev, NIC_DRIVER);
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
	(*nic.dev.disable)(&nic.dev);
}
