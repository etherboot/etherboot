 /*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#ifndef	NIC_H
#define NIC_H

struct nic_id
{
	uint8_t		len;
	uint8_t		bus_type;
#define	PCI_BUS_TYPE	1
#define	ISA_BUS_TYPE	2
	uint16_t	vendor_id;
	uint16_t	device_id;
};

/* Dont use sizeof, that will include the padding */
#define	NIC_ID_SIZE	6

/*
 *	Structure returned from eth_probe and passed to other driver
 *	functions.
 */

struct nic
{
	void		(*reset)P((struct nic *));
	int		(*poll)P((struct nic *));
	void		(*transmit)P((struct nic *, const char *d,
				unsigned int t, unsigned int s, const char *p));
	void		(*disable)P((struct nic *));
	int		flags;	/* driver specific flags */
	struct rom_info	*rom_info;	/* -> rom_info from main */
	unsigned char	*node_addr;
	char		*packet;
	unsigned int	packetlen;
	struct nic_id	devid;	/* NIC device ID string (sent to DHCP server) */
	void		*priv_data;	/* driver can hang private data here */
};

#endif	/* NIC_H */
