#if !defined(PCI_H) && defined(CONFIG_PCI)
#define PCI_H

/*
** Support for NE2000 PCI clones added David Monro June 1997
** Generalised for other PCI NICs by Ken Yap July 1997
**
** Most of this is taken from:
**
** /usr/src/linux/drivers/pci/pci.c
** /usr/src/linux/include/linux/pci.h
** /usr/src/linux/arch/i386/bios32.c
** /usr/src/linux/include/linux/bios32.h
** /usr/src/linux/drivers/net/ne.c
*/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include "pci_ids.h"

#define PCI_COMMAND_IO			0x1	/* Enable response in I/O space */
#define PCI_COMMAND_MEM			0x2	/* Enable response in mem space */
#define PCI_COMMAND_MASTER		0x4	/* Enable bus mastering */
#define PCI_LATENCY_TIMER		0x0d	/* 8 bits */
#define PCI_COMMAND_SPECIAL		0x8	/* Enable response to special cycles */
#define PCI_COMMAND_INVALIDATE		0x10	/* Use memory write and invalidate */

#define PCIBIOS_PCI_FUNCTION_ID         0xb1XX
#define PCIBIOS_PCI_BIOS_PRESENT        0xb101
#define PCIBIOS_FIND_PCI_DEVICE         0xb102
#define PCIBIOS_FIND_PCI_CLASS_CODE     0xb103
#define PCIBIOS_GENERATE_SPECIAL_CYCLE  0xb106
#define PCIBIOS_READ_CONFIG_BYTE        0xb108
#define PCIBIOS_READ_CONFIG_WORD        0xb109
#define PCIBIOS_READ_CONFIG_DWORD       0xb10a
#define PCIBIOS_WRITE_CONFIG_BYTE       0xb10b
#define PCIBIOS_WRITE_CONFIG_WORD       0xb10c
#define PCIBIOS_WRITE_CONFIG_DWORD      0xb10d

#define PCI_VENDOR_ID           0x00    /* 16 bits */
#define PCI_DEVICE_ID           0x02    /* 16 bits */
#define PCI_COMMAND             0x04    /* 16 bits */

#define PCI_REVISION            0x08    /* 8 bits  */
#define PCI_REVISION_ID         0x08    /* 8 bits  */
#define PCI_CLASS_REVISION      0x08    /* 32 bits  */
#define PCI_CLASS_CODE          0x0b    /* 8 bits */
#define PCI_SUBCLASS_CODE       0x0a    /* 8 bits */
#define PCI_HEADER_TYPE         0x0e    /* 8 bits */


/* Header type 0 (normal devices) */
#define PCI_CARDBUS_CIS		0x28
#define PCI_SUBSYSTEM_VENDOR_ID	0x2c
#define PCI_SUBSYSTEM_ID	0x2e  

#define PCI_BASE_ADDRESS_0      0x10    /* 32 bits */
#define PCI_BASE_ADDRESS_1      0x14    /* 32 bits */
#define PCI_BASE_ADDRESS_2      0x18    /* 32 bits */
#define PCI_BASE_ADDRESS_3      0x1c    /* 32 bits */
#define PCI_BASE_ADDRESS_4      0x20    /* 32 bits */
#define PCI_BASE_ADDRESS_5      0x24    /* 32 bits */

#ifndef	PCI_BASE_ADDRESS_IO_MASK
#define	PCI_BASE_ADDRESS_IO_MASK       (~0x03)
#endif
#ifndef	PCI_BASE_ADDRESS_MEM_MASK
#define	PCI_BASE_ADDRESS_MEM_MASK       (~0x0f)
#endif
#define	PCI_BASE_ADDRESS_SPACE_IO	0x01
#define	PCI_ROM_ADDRESS		0x30	/* 32 bits */
#define	PCI_ROM_ADDRESS_ENABLE	0x01	/* Write 1 to enable ROM,
					   bits 31..11 are address,
					   10..2 are reserved */

/* Header type 1 (PCI-to-PCI bridges) */
#define PCI_PRIMARY_BUS		0x18	/* Primary bus number */
#define PCI_SECONDARY_BUS	0x19	/* Secondary bus number */
#define PCI_SUBORDINATE_BUS	0x1a	/* Highest bus number behind the bridge */
#define PCI_SEC_LATENCY_TIMER	0x1b	/* Latency timer for secondary interface */
#define PCI_IO_BASE		0x1c	/* I/O range behind the bridge */
#define PCI_IO_LIMIT		0x1d
#define  PCI_IO_RANGE_TYPE_MASK	0x0f	/* I/O bridging type */
#define  PCI_IO_RANGE_TYPE_16	0x00
#define  PCI_IO_RANGE_TYPE_32	0x01
#define  PCI_IO_RANGE_MASK	~0x0f
#define PCI_SEC_STATUS		0x1e	/* Secondary status register, only bit 14 used */
#define PCI_MEMORY_BASE		0x20	/* Memory range behind */
#define PCI_MEMORY_LIMIT	0x22
#define  PCI_MEMORY_RANGE_TYPE_MASK 0x0f
#define  PCI_MEMORY_RANGE_MASK	~0x0f
#define PCI_PREF_MEMORY_BASE	0x24	/* Prefetchable memory range behind */
#define PCI_PREF_MEMORY_LIMIT	0x26
#define  PCI_PREF_RANGE_TYPE_MASK 0x0f
#define  PCI_PREF_RANGE_TYPE_32	0x00
#define  PCI_PREF_RANGE_TYPE_64	0x01
#define  PCI_PREF_RANGE_MASK	~0x0f
#define PCI_PREF_BASE_UPPER32	0x28	/* Upper half of prefetchable memory range */
#define PCI_PREF_LIMIT_UPPER32	0x2c
#define PCI_IO_BASE_UPPER16	0x30	/* Upper half of I/O addresses */
#define PCI_IO_LIMIT_UPPER16	0x32
/* 0x34 same as for htype 0 */
/* 0x35-0x3b is reserved */
#define PCI_ROM_ADDRESS1	0x38	/* Same as PCI_ROM_ADDRESS, but for htype 1 */
/* 0x3c-0x3d are same as for htype 0 */
#define PCI_BRIDGE_CONTROL	0x3e
#define  PCI_BRIDGE_CTL_PARITY	0x01	/* Enable parity detection on secondary interface */
#define  PCI_BRIDGE_CTL_SERR	0x02	/* The same for SERR forwarding */
#define  PCI_BRIDGE_CTL_NO_ISA	0x04	/* Disable bridging of ISA ports */
#define  PCI_BRIDGE_CTL_VGA	0x08	/* Forward VGA addresses */
#define  PCI_BRIDGE_CTL_MASTER_ABORT 0x20  /* Report master aborts */
#define  PCI_BRIDGE_CTL_BUS_RESET 0x40	/* Secondary bus reset */
#define  PCI_BRIDGE_CTL_FAST_BACK 0x80	/* Fast Back2Back enabled on secondary interface */

#define PCI_SLOT(devfn)		  ((devfn) >> 3)
#define PCI_FUNC(devfn)           ((devfn) & 0x07)

#define BIOS32_SIGNATURE        (('_' << 0) + ('3' << 8) + ('2' << 16) + ('_' << 24))

/* PCI signature: "PCI " */
#define PCI_SIGNATURE           (('P' << 0) + ('C' << 8) + ('I' << 16) + (' ' << 24))

/* PCI service signature: "$PCI" */
#define PCI_SERVICE             (('$' << 0) + ('P' << 8) + ('C' << 16) + ('I' << 24))

union bios32 {
	struct {
		unsigned long signature;	/* _32_ */
		unsigned long entry;		/* 32 bit physical address */
		unsigned char revision;		/* Revision level, 0 */
		unsigned char length;		/* Length in paragraphs should be 01 */
		unsigned char checksum;		/* All bytes must add up to zero */
		unsigned char reserved[5];	/* Must be zero */
	} fields;
	char chars[16];
};

struct pci_device;
struct dev;
typedef int (*pci_probe_t)(struct dev *, struct pci_device *);

struct pci_device {
	uint32_t		class;
	uint16_t		vendor, dev_id;
	const char		*name;
	/* membase and ioaddr are silly and depricated */
	unsigned int		membase;
	unsigned int		ioaddr;
	unsigned int		romaddr;
	unsigned char		devfn;
	unsigned char		bus;
	const struct pci_driver	*driver;
};

extern void scan_pci_bus(int type, struct pci_device *dev);
extern void find_pci(int type, struct pci_device *dev);

extern int pcibios_read_config_byte(unsigned int bus, unsigned int device_fn, unsigned int where, uint8_t *value);
extern int pcibios_write_config_byte (unsigned int bus, unsigned int device_fn, unsigned int where, uint8_t value);
extern int pcibios_read_config_word(unsigned int bus, unsigned int device_fn, unsigned int where, uint16_t *value);
extern int pcibios_write_config_word (unsigned int bus, unsigned int device_fn, unsigned int where, uint16_t value);
extern int pcibios_read_config_dword(unsigned int bus, unsigned int device_fn, unsigned int where, uint32_t *value);
extern int pcibios_write_config_dword(unsigned int bus, unsigned int device_fn, unsigned int where, uint32_t value);
void adjust_pci_device(struct pci_device *p);


static inline int 
pci_read_config_byte(struct pci_device *dev, unsigned int where, uint8_t *value)
{
	return pcibios_read_config_byte(dev->bus, dev->devfn, where, value);
}
static inline int 
pci_write_config_byte(struct pci_device *dev, unsigned int where, uint8_t value)
{
	return pcibios_write_config_byte(dev->bus, dev->devfn, where, value);
}
static inline int 
pci_read_config_word(struct pci_device *dev, unsigned int where, uint16_t *value)
{
	return pcibios_read_config_word(dev->bus, dev->devfn, where, value);
}
static inline int 
pci_write_config_word(struct pci_device *dev, unsigned int where, uint16_t value)
{
	return pcibios_write_config_word(dev->bus, dev->devfn, where, value);
}
static inline int 
pci_read_config_dword(struct pci_device *dev, unsigned int where, uint32_t *value)
{
	return pcibios_read_config_dword(dev->bus, dev->devfn, where, value);
}
static inline int 
pci_write_config_dword(struct pci_device *dev, unsigned int where, uint32_t value)
{
	return pcibios_write_config_dword(dev->bus, dev->devfn, where, value);
}

/* Helper functions to find the size of a pci bar */
extern unsigned long pci_bar_start(struct pci_device *dev, unsigned int bar);
extern unsigned long pci_bar_size(struct pci_device *dev, unsigned int bar);

struct pci_id {
	unsigned short vendor, dev_id;
	const char *name;
};

struct dev;
/* Most pci drivers will use this */
struct pci_driver {
	int type;
	const char *name;
	pci_probe_t probe;
	struct pci_id *ids;
	int id_count;

/* On a few occasions the hardware is standardized enough that
 * we only need to know the class of the device and not the exact
 * type to drive the device correctly.  If this is the case
 * set a class value other than 0.
 */
	unsigned short class;
};

#define __pci_driver	__attribute__ ((unused,__section__(".drivers.pci")))
/* Defined by the linker... */
extern const struct pci_driver pci_drivers[];
extern const struct pci_driver pci_drivers_end[];


#endif	/* PCI_H */
