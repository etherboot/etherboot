#ifdef CONFIG_PCI

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include "etherboot.h"
#include "pci.h"

/*#define	DEBUG	1*/

static void scan_drivers(
	int type, 
	uint32_t class, uint16_t vendor, uint16_t device,
	const struct pci_driver *last_driver, struct pci_device *dev)
{
	const struct pci_driver *skip_driver = last_driver;
	
	/* Assume there is only one match of the correct type */
	const struct pci_driver *driver;
	for(driver = pci_drivers; driver < pci_drivers_end; driver++) {
		int i;
		if (driver->type != type)
			continue;
		if (skip_driver) {
			if (skip_driver == driver) 
				skip_driver = 0;
			continue;
		}
		for(i = 0; i < driver->id_count; i++) {
			if ((vendor == driver->ids[i].vendor) &&
				(device == driver->ids[i].dev_id)) {

				dev->driver = driver;
#if 0
				dev->name   = driver->ids[i].name;
#else
				dev->name   = driver->name;
#endif
				goto out;
			}
		}
	}
	for(driver = pci_drivers; driver < pci_drivers_end; driver++) {
		if (driver->type != type)
			continue;
		if (skip_driver) {
			if (skip_driver == driver)
				skip_driver = 0;
			continue;
		}
		if (last_driver == driver)
			continue;
		if ((class >> 8) == driver->class) {
			dev->driver = driver;
			dev->name   = driver->name;
			goto out;
		}
	}
 out:
	return;
}

void scan_pci_bus(int type, struct pci_device *dev)
{
	unsigned int first_bus, first_devfn;
	const struct pci_driver *first_driver;
	unsigned int devfn, bus, buses;
	unsigned char hdr_type = 0;
	uint32_t class;
	uint16_t vendor, device;
	uint32_t l, membase, ioaddr, romaddr;
	int reg;

	first_bus    = 0;
	first_devfn  = 0;
	first_driver = 0;
	if (dev->driver) {
		first_driver = dev->driver;
		first_bus    = dev->bus;
		first_devfn  = dev->devfn;
		dev->driver  = 0;
		dev->bus     = 0;
		dev->devfn   = 0;
	}
		
	/* Scan all PCI buses, until we find our card.
	 * We could be smart only scan the required buses but that
	 * is error prone, and tricky.
	 * By scanning all possible pci buses in order we should find
	 * our card eventually. 
	 */
	buses=256;
	for (bus = first_bus; bus < buses; ++bus) {
		for (devfn = first_devfn; devfn < 0xff; ++devfn) {
			if (PCI_FUNC (devfn) == 0)
				pcibios_read_config_byte(bus, devfn, PCI_HEADER_TYPE, &hdr_type);
			else if (!(hdr_type & 0x80))	/* not a multi-function device */
				continue;
			pcibios_read_config_dword(bus, devfn, PCI_VENDOR_ID, &l);
			/* some broken boards return 0 if a slot is empty: */
			if (l == 0xffffffff || l == 0x00000000) {
				continue;
			}
			vendor = l & 0xffff;
			device = (l >> 16) & 0xffff;

			pcibios_read_config_dword(bus, devfn, PCI_REVISION, &l);
			class = (l >> 8) & 0xffffff;
#if	DEBUG
		{
			int i;
			printf("%hhx:%hhx.%hhx [%hX/%hX]\n",
				bus, PCI_SLOT(devfn), PCI_FUNC(devfn),
				vendor, device);
#if	DEBUG > 1
			for(i = 0; i < 256; i++) {
				unsigned char byte;
				if ((i & 0xf) == 0) {
					printf("%hhx: ", i);
				}
				pcibios_read_config_byte(bus, devfn, i, &byte);
				printf("%hhx ", byte);
				if ((i & 0xf) == 0xf) {
					printf("\n");
				}
			}
#endif

		}
#endif
			scan_drivers(type, class, vendor, device, first_driver, dev);
			first_driver = 0;
			if (!dev->driver)
				continue;

			dev->devfn = devfn;
			dev->bus = bus;
			dev->class = class;
			dev->vendor = vendor;
			dev->dev_id = device;
			
			
			/* Get the ROM base address */
			pcibios_read_config_dword(bus, devfn, 
				PCI_ROM_ADDRESS, &romaddr);
			romaddr >>= 10;
			dev->romaddr = romaddr;
			
			/* Get the ``membase'' */
			pcibios_read_config_dword(bus, devfn,
				PCI_BASE_ADDRESS_1, &membase);
			dev->membase = membase;
				
			/* Get the ``ioaddr'' */
			for (reg = PCI_BASE_ADDRESS_0; reg <= PCI_BASE_ADDRESS_5; reg += 4) {
				pcibios_read_config_dword(bus, devfn, reg, &ioaddr);
				if ((ioaddr & PCI_BASE_ADDRESS_IO_MASK) == 0 || (ioaddr & PCI_BASE_ADDRESS_SPACE_IO) == 0)
					continue;
				
				
				/* Strip the I/O address out of the returned value */
				ioaddr &= PCI_BASE_ADDRESS_IO_MASK;
				
				/* Take the first one or the one that matches in boot ROM address */
				dev->ioaddr = ioaddr;
			}
#if 0
			printf("Found %s ROM address %#hx\n",
				dev->name, romaddr);
#endif
			return;
		}
		first_devfn = 0;
	}
	first_bus = 0;
}



/*
 *	Set device to be a busmaster in case BIOS neglected to do so.
 *	Also adjust PCI latency timer to a reasonable value, 32.
 */
void adjust_pci_device(struct pci_device *p)
{
	unsigned short	new_command, pci_command;
	unsigned char	pci_latency;

	pcibios_read_config_word(p->bus, p->devfn, PCI_COMMAND, &pci_command);
	new_command = pci_command | PCI_COMMAND_MASTER|PCI_COMMAND_IO;
	if (pci_command != new_command) {
		printf(
			"The PCI BIOS has not enabled this device!\n"
			"Updating PCI command %hX->%hX. pci_bus %hhX pci_device_fn %hhX\n",
			   pci_command, new_command, p->bus, p->devfn);
		pcibios_write_config_word(p->bus, p->devfn, PCI_COMMAND, new_command);
	}
	pcibios_read_config_byte(p->bus, p->devfn, PCI_LATENCY_TIMER, &pci_latency);
	if (pci_latency < 32) {
		printf("PCI latency timer (CFLT) is unreasonably low at %d. Setting to 32 clocks.\n", 
			pci_latency);
		pcibios_write_config_byte(p->bus, p->devfn, PCI_LATENCY_TIMER, 32);
	}
}

/*
 * Find the start of a pci resource.
 */
unsigned long pci_bar_start(struct pci_device *dev, unsigned int bar)
{
	uint32_t start;
	pci_read_config_dword(dev, bar, &start);
	if (start & PCI_BASE_ADDRESS_SPACE_IO) {
		start &= PCI_BASE_ADDRESS_IO_MASK;
	} else {
		start &= PCI_BASE_ADDRESS_MEM_MASK;
	}
	return start + pcibios_bus_base(dev->bus);
}

/*
 * Find the size of a pci resource.
 */
unsigned long pci_bar_size(struct pci_device *dev, unsigned int bar)
{
	uint32_t start, size;
	/* Save the original bar */
	pci_read_config_dword(dev, bar, &start);
	/* Compute which bits can be set */
	pci_write_config_dword(dev, bar, ~0);
	pci_read_config_dword(dev, bar, &size);
	/* Restore the original size */
	pci_write_config_dword(dev, bar, start);
	/* Find the significant bits */
	if (start & PCI_BASE_ADDRESS_SPACE_IO) {
		size &= PCI_BASE_ADDRESS_IO_MASK;
	} else {
		size &= PCI_BASE_ADDRESS_MEM_MASK;
	}
	/* Find the lowest bit set */
	size = size & ~(size - 1);
	return size;
}

#endif /* CONFIG_PCI */
