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
		0,				/* dev.disable */
		{
			DEV_ID_SIZE-1,
			RFC1533_VENDOR_NIC_DEV_ID,
			5,
			PCI_BUS_TYPE,
			0,
			0
		},				/* dev.devid */
		0,				/* index */
		PROBE_NONE,			/* to_probe */
		{},				/* state */
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
	for(driver = pci_drivers; driver < pci_drivers_end; driver++) {
		printf("[%s]", driver->name);
	}
}
#endif	
#ifdef CONFIG_ISA
{
	const struct isa_driver *driver;
	for(driver = isa_drivers; driver < isa_drivers_end; driver++) {
		printf("[%s]", driver->name);
	}
}
#endif	
	putchar('\n');
}

#ifdef CONFIG_PCI
int pci_probe(int last_adapter, struct dev *dev, int type)
{
/*
 *	NIC probing is in pci device order, followed by the 
 *      link order of the drivers.  A driver that matches 
 *      on vendor and device id will supersede a driver
 *      that matches on pci class.
 *
 *	If you want to probe for another device behind the same pci
 *      device just increment index.  And the previous probe call
 *      will be repeated.
 */
	struct pci_probe_state *state = &dev->state.pci;
	if (last_adapter == -1) {
		state->advance = 1;
		state->dev.driver = 0;
		state->dev.bus    = 0;
		state->dev.devfn  = 0;
	}
	for(;;) {
		if (state->advance) {
			find_pci(type, &state->dev);
			dev->index = 0;
		}
		state->advance = 1;
		
		if (state->dev.driver == 0)
			return -1;
		
#if 0
		/* FIXME the romaddr code needs a total rethought to be useful */
		if (state->dev.romaddr != ((unsigned long) rom.rom_segment << 4)) {
			continue;
		}
#endif
		dev->devid.bus_type = PCI_BUS_TYPE;
		dev->devid.vendor_id = htons(state->dev.vendor);
		dev->devid.device_id = htons(state->dev.dev_id);
		
		printf("[%s]", state->dev.name);
		if (state->dev.driver->probe(dev, &state->dev)) {
			state->advance = !dev->index;
			return 0;
		}
	}
	return -1;
}
#else
#define pci_probe(l, d, t) (-1)
#endif

#ifdef CONFIG_ISA
int isa_probe(int last_adapter, struct dev *dev, int type)
{
/*
 *	NIC probing is in the order the drivers were linked togeter.
 *	If for some reason you want to change the order,
 *	just change the order you list the drivers in.
 */
	struct isa_probe_state *state = &dev->state.isa;
	if (last_adapter == -1) {
		state->driver = isa_drivers;
		state->index = 0;
	}
	for(; state->driver < isa_drivers_end; state->driver++)
	{
		if (state->driver->type != type)
			continue;
		printf("[%s]", state->driver->name);
		dev->devid.bus_type = ISA_BUS_TYPE;
		dev->index = state->index;
		/* driver will fill in vendor and device IDs */
		if (state->driver->probe(dev, state->driver->ioaddrs)) {
			state->index = dev->index;
			if (dev->index == 0)
				state->driver++;
			return 0;
		}
	}
	return -1;
}
#else
#define isa_probe(l, d, t) (-1)
#endif

int probe(int adapter, struct dev *dev, int type)
{
	
#ifndef TRY_ALL_DEVICES
	adapter = -1;
#endif
	if (adapter == -1) {
		dev->to_probe = PROBE_PCI;
		memset(&dev->state, 0, sizeof(dev->state));
	}
	if (dev->to_probe == PROBE_PCI) {
		printf("Probing pci...");
		adapter = pci_probe(adapter, dev, type);
		printf("\n");
		if (adapter == -1) {
			dev->to_probe = PROBE_ISA;
		}
	}
	if (dev->to_probe == PROBE_ISA) {
		printf("Probing isa...");
		adapter = isa_probe(adapter, dev, type);
		printf("\n");
		if (adapter == -1) {
			dev->to_probe = PROBE_NONE;
		}
	}
	return adapter;
}

void disable(struct dev *dev)
{
	if (dev->disable) {
		dev->disable(dev);
		dev->disable = 0;
	}
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
	disable(&nic.dev);
}
