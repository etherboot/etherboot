#ifndef DEV_H
#define DEV_H

#include "isa.h"
#include "pci.h"

/* Need to check the packing of this struct if Etherboot is ported */
struct dev_id
{
	uint8_t		encap_len;
	uint8_t		tag;
	uint8_t		len;
	uint8_t		bus_type;
#define	PCI_BUS_TYPE	1
#define	ISA_BUS_TYPE	2
	uint16_t	vendor_id;
	uint16_t	device_id;
};

/* Dont use sizeof, that will include the padding */
#define	DEV_ID_SIZE	8


struct pci_probe_state 
{
#ifdef CONFIG_PCI
	struct pci_device dev;
	int advance;
#else
	int dummy;
#endif
};
struct isa_probe_state
{
#ifdef CONFIG_ISA
	struct isa_driver *driver;
	int index;
#else
	int dummy;
#endif
};

union probe_state
{
	struct pci_probe_state pci;
	struct isa_probe_state isa;
};

struct dev
{
	void		(*disable)P((struct dev *));
	struct dev_id	devid;	/* device ID string (sent to DHCP server) */
	int		index;  /* Index of next device on this controller to probe */
	int 		to_probe;
#define	PROBE_NONE 0
#define PROBE_PCI  1
#define PROBE_ISA  2
	union probe_state state;
};

#define NIC_DRIVER    1
#define BRIDGE_DRIVER 2
#define DISK_DRIVER   3
#define FLOPPY_DRIVER 4

extern int probe(int last_adapter, struct dev *dev, int type);
extern void disable(struct dev *dev);

#endif /* DEV_H */
