#ifdef ALLMULTICAST
#error multicast support is not yet implemented
#endif
/**************************************************************************
*
*    sundance.c -- Etherboot device driver for the Sundance ST201 "Alta".
*    Written 2002-2002 by Timothy Legge <tlegge@rogers.com>
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*    Portions of this code based on:
*               sundance.c: A Linux device driver for the Sundance ST201 "Alta"
*               Written 1999-2002 by Donald Becker
*
*               tulip.c: Tulip and Clone Etherboot Driver
*               By Marty Conner 
*               Copyright (C) 2001 Entity Cyber, Inc.
*               
*    
*
*    REVISION HISTORY:
*    ================
*    v0.10	01-01-2003	TJL	Initial implementation Beta.
***************************************************************************/

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the PCI support functions, if this is a PCI NIC */
#include "pci.h"
#include "timer.h"
/* NIC specific static variables go here */

static int max_interrupt_work = 20;
static int multicast_filter_limit = 32;
static int rx_copybreak = 0;

#define MAX_UNITS 8
static int options[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int full_duplex[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };

enum alta_offsets {
    TxListPtr = 0x04,
    TxDescPoll = 0x0a,
    RxListPtr = 0x10,
    RxDescPoll = 0x16,
    ASICCtrl = 0x30,
    EEData = 0x34,
    EECtrl = 0x36,
    TxStatus = 0x46,
    DownCounter = 0x18,
    IntrEnable = 0x4c,
    IntrStatus = 0x4e,
    MACCtrl0 = 0x50,
    MACCtrl1 = 0x52,
    StationAddr = 0x54,
    MaxFrameSize = 0x5a,
    RxMode = 0x5c,
    MIICtrl = 0x5e,
    RxStatus = 0x0c,

};
enum intr_status_bits {
    IntrSummary = 0x0001, IntrPCIErr = 0x0002, IntrMACCtrl = 0x0008,
    IntrTxDone = 0x0004, IntrRxDone = 0x0010, IntrRxStart = 0x0020,
    IntrDrvRqst = 0x0040,
    StatsMax = 0x0080, LinkChange = 0x0100,
    IntrTxDMADone = 0x0200, IntrRxDMADone = 0x0400,
};

enum rx_mode_bits {
    AcceptAllIPMulti = 0x20, AcceptMultiHash = 0x10, AcceptAll = 0x08,
    AcceptBroadcast = 0x04, AcceptMulticast = 0x02, AcceptMyPhys = 0x01,
};
enum mac_ctrl0_bits {
    EnbFullDuplex = 0x20,
};
enum mac_ctrl1_bits {
    StatsEnable = 0x0020, StatsDisable = 0x0040, StatsEnabled = 0x0080,
    TxEnable = 0x0100, TxDisable = 0x0200, TxEnabled = 0x0400,
    RxEnable = 0x0800, RxDisable = 0x1000, RxEnabled = 0x2000,
};

/* Descriptor Status Bits */
enum des_status_bits {
    DescOwn = 0x8000,
    DescEndPacket = 0x4000,
    DescEndRing = 0x2000,
    LastFrag = 0x80000000,
    DescIntrOnTx = 0x8000,
    DescIntrOnDMADone = 0x80000000,
    DisableAlign = 0x00000001,
};

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

#define virt_to_le32desc(addr) cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr) bus_to_virt(le32_to_cpu(addr))

#define MII_CNT		4

#define BUFLEN 1536
#define PKT_BUF_SZ	1536


/* Define the transmit and receive descriptor */
struct netdev_desc {
    u32 next_desc;
    u32 status;
    u32 addr;
    u32 length;
};
#define TX_RING_SIZE	2

static struct netdev_desc tx_ring[TX_RING_SIZE]
    __attribute__ ((aligned(4)));

static unsigned char txb[BUFLEN * TX_RING_SIZE]
    __attribute__ ((aligned(4)));

#define RX_RING_SIZE	4

static struct netdev_desc rx_ring[RX_RING_SIZE]
    __attribute__ ((aligned(4)));

static unsigned char rxb[RX_RING_SIZE * BUFLEN]
    __attribute__ ((aligned(4)));

#define TX_TIME_OUT	2*TICKS_PER_SEC

/*******************
* Global Storage 
*******************/
static int mii_preamble_required = 0;
static u32 BASE;
#define EEPROM_SIZE	128
u8 ee_data[EEPROM_SIZE];

struct sundance_private {
    unsigned short vendor_id;	/* PCI Vendor code */
    unsigned short dev_id;	/* PCI Device code */
    unsigned char ethr[ETH_HLEN];	/* Buffer for ethernet header */
    const char *nic_name;
    unsigned int if_port;
    /* Frequently used values */
    int msg_level;
    int chip_id, drv_flags;

    int max_interrupt_word;

    unsigned int cur_rx, dirty_rx;	/* Producer/consumer ring indicies */
    unsigned rx_buf_sz;		/* Based on mtu + Slack */
    int rx_copybreak;

    struct netdev_desc *rx_head_desc;
    unsigned int cur_tx, dirty_tx;
    unsigned int tx_full:1;


    /* These values keep track of the tranceiver/media in use */
    unsigned int full_duplex:1;	/* Full Duplex Requested */
    unsigned int duplex_lock:1;
    unsigned int medialock:1;	/* Do not sense media */
    unsigned int default_port:4;

    unsigned int an_enable:1;
    unsigned int speed;

    /* Multicast and receive mode */
    /* spinloc_t mcastlock */
    u16 mcast_filter[4];
    int multicast_filter_limit;

    /* MII tranceiver section */
    int mii_cnt;		/* MII device addresses */
    int link_status;
    u16 advertizing;		/* NWay media advertizing */
    unsigned char phys[2];


    int saved_if_port;
} sdx;

static struct sundance_private *sdc;

/* Station Address location within the EEPROM */
#define EEPROM_SA_OFFSET	0x10



static int eeprom_read(long ioaddr, int location);
static int mdio_read(struct nic *nic, int phy_id, int location);
static void mdio_write(struct nic *nic, int phy_id, int location,
		       int value);
static void set_rx_mode(struct nic *nic);

static void check_duplex(struct nic *nic)
{
    int mii_reg5 = mdio_read(nic, sdc->phys[0], 5);
    int negociated = mii_reg5 & sdc->advertizing;
    int duplex;

    if (sdc->duplex_lock || mii_reg5 == 0xffff)
	return;
    duplex = (negociated & 0x0100) || (negociated & 0x01C0) == 0x0040;
    if (sdc->full_duplex != duplex) {
	sdc->full_duplex = duplex;
	printf("%s: Setting %s-duplex based on MII #%d "
	       "negociated capability %hX\n", sdc->nic_name,
	       duplex ? "full" : "half", sdc->phys[0], negociated);
	outw(duplex ? 0x20 : 0, BASE + MACCtrl0);

    }
}


/**************************************************************************
 *  init_ring - setup the tx and rx descriptors
 *************************************************************************/
static void init_ring(struct nic *nic)
{
    int i;

    sdc->cur_rx = 0;
    sdc->rx_buf_sz = (PKT_BUF_SZ);
    sdc->rx_head_desc = &rx_ring[0];

    /* Initialize all the Rx descriptors */
    for (i = 0; i < RX_RING_SIZE; i++) {
	rx_ring[i].next_desc = virt_to_le32desc(&rx_ring[i + 1]);
	rx_ring[i].status = 0;
	rx_ring[i].length = 0;
	rx_ring[i].addr = 0;
    }

    /* Mark the last entry as wrapping the ring */
    rx_ring[i - 1].next_desc = virt_to_le32desc(&rx_ring[0]);

    for (i = 0; i < RX_RING_SIZE; i++) {
	rx_ring[i].addr = virt_to_le32desc(&rxb[i * BUFLEN]);
	rx_ring[i].length = cpu_to_le32(sdc->rx_buf_sz | LastFrag);
    }
    sdc->dirty_rx = (unsigned int) (i - RX_RING_SIZE);

    /* We only use one transmit buffer, but two descriptors so
     * so transmit engines have somewhere to point should they feel the need */
    tx_ring[0].status = 0;
    tx_ring[0].addr = virt_to_bus(&txb[0]);
    tx_ring[0].next_desc = virt_to_bus(&tx_ring[1]);

    /* This descriptor is never used */
    tx_ring[1].status = 0;
    tx_ring[1].addr = virt_to_bus(&txb[0]);
    tx_ring[1].next_desc = virt_to_bus(&tx_ring[0]);

/* Mark the last entry as wrapping the ring, though this should never happen */
    tx_ring[1].length = cpu_to_le32(LastFrag | BUFLEN);
}

/**************************************************************************
 *  RESET - Reset Adapter
 * ***********************************************************************/
static void sundance_reset(struct nic *nic)
{
    int i;

    init_ring(nic);

    sdc->full_duplex = sdc->duplex_lock;

    outl(virt_to_bus(&rx_ring[sdc->cur_rx % RX_RING_SIZE]),
	 BASE + RxListPtr);
    /* The Tx List Pointer is written as packets are queued */

    /* Write the MAC address to the StationAddress */
    for (i = 0; i < 6; i++)
	outb(nic->node_addr[i], BASE + StationAddr + i);

    /* Get the link status */
    sdc->link_status = inb(BASE + MIICtrl) & 0xE0;
    /* Write the status to the MACCtrl0 register */
    outw((sdc->full_duplex || (sdc->link_status & 0x20)) ? 0x120 : 0,
	 BASE + MACCtrl0);
    /* FIXME Do we need to set the mtu here ? 
     * It defaults to 1514 upon reset */
    outw(1514, BASE + MaxFrameSize);
    if (1514 > 2047)
	outl(inl(BASE + ASICCtrl) | 0x0c, BASE + ASICCtrl);

    set_rx_mode(nic);
    outw(0, BASE + DownCounter);
    /* Set the chip to poll every N*30nsec */
    outb(100, BASE + RxDescPoll);
    outb(127, BASE + TxDescPoll);


    /* Enable interupts by setting the interrupt mask */
    outw(IntrRxDMADone | IntrPCIErr | IntrDrvRqst | IntrTxDone
	 | StatsMax | LinkChange, BASE + IntrEnable);
    outw(StatsEnable | RxEnable | TxEnable, BASE + MACCtrl1);

    /* Construct a perfect filter frame with the mac address as first match
     * and broadcast for all others */
    for (i = 0; i < 192; i++)
	txb[i] = 0xFF;

    txb[0] = nic->node_addr[0];
    txb[1] = nic->node_addr[1];
    txb[2] = nic->node_addr[2];
    txb[3] = nic->node_addr[3];
    txb[4] = nic->node_addr[4];
    txb[5] = nic->node_addr[5];

    check_duplex(nic);
    printf("%s: Done sundance_reset, status: Rx %hX Tx %hX "
	   "MAC Control %hX, %hX %hX\n",
	   sdc->nic_name, (int) inl(BASE + RxStatus),
	   (int) inw(BASE + TxStatus), (int) inl(BASE + MACCtrl0),
	   (int) inw(BASE + MACCtrl1), (int) inw(BASE + MACCtrl0));
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int sundance_poll(struct nic *nic)
{
    /* return true if there's an ethernet packet ready to read */
    /* nic->packet should contain data on return */
    /* nic->packetlen should contain length of data */
    u32 frame_status;
    struct netdev_desc *desc;
/*	int intr_status = inw(BASE + IntrStatus);
	
	if((intr_status & ~IntrRxDone) == 0 || intr_status == 0xffff)
		return 0;

	outw(intr_status & (IntrRxDMADone ), BASE + IntrStatus);
	
	if(!(intr_status & IntrRxDMADone))
		return 0;
*/
    if (!(sdc->rx_head_desc->status & cpu_to_le32(DescOwn)))
	return 0;

    (struct netdev_desc *) desc = sdc->rx_head_desc;
    frame_status = le32_to_cpu(desc->status);
    nic->packetlen = (frame_status & 0x1fff);

    /* Throw away a corrupted packet and move on */
    if (frame_status & 0x001f4000) {
	/* return the descriptor buffer to receive ring */
	rx_ring[sdc->cur_rx].status = 0x00000000;
	sdc->cur_rx = (++sdc->cur_rx) % RX_RING_SIZE;
	return 0;
    }

    /* Copy the packet to working buffer */
    memcpy(nic->packet, rxb + (sdc->cur_rx * BUFLEN), nic->packetlen);

    desc->status = 0x00000000;
    /* return the descriptor and buffer to receive ring */
    rx_ring[sdc->cur_rx].length = cpu_to_le32(sdc->rx_buf_sz | LastFrag);
    rx_ring[sdc->cur_rx].status = 0;

    sdc->cur_rx = (++sdc->cur_rx) % RX_RING_SIZE;
    sdc->rx_head_desc = &rx_ring[sdc->cur_rx];


    return 1;

}



/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void sundance_transmit(struct nic *nic, const char *d,	/* Destination */
			      unsigned int t,	/* Type */
			      unsigned int s,	/* size */
			      const char *p)
{				/* Packet */
    u16 nstype;
    u32 to;
/*	int intr_status = inw(BASE + IntrStatus);
	if ((intr_status & ~IntrRxDone) == 0 || intr_status == 0xffff)
		return;

	outw(intr_status & (IntrRxDMADone | IntrPCIErr | 
				IntrDrvRqst | IntrTxDone | IntrTxDMADone | 
				StatsMax | LinkChange), BASE + IntrStatus);

	if(!(intr_status & IntrTxDone))
		return;
*/
    /* Disable the Tx */
    outw(TxDisable, BASE + MACCtrl1);

    memcpy(txb, d, ETH_ALEN);
    memcpy(txb + ETH_ALEN, nic->node_addr, ETH_ALEN);
    nstype = htons((u16) t);
    memcpy(txb + 2 * ETH_ALEN, (u8 *) & nstype, 2);
    memcpy(txb + ETH_HLEN, p, s);

    s += ETH_HLEN;
    s &= 0x0FFF;
    while (s < ETH_ZLEN)
	txb[s++] = '\0';

    /* Setup the transmit descriptor */
    tx_ring[0].length = cpu_to_le32(s | LastFrag);
    tx_ring[0].status = cpu_to_le32(0x80000000 | DisableAlign);
    outl(virt_to_bus(&tx_ring[0]), BASE + TxListPtr);
    /* Enable Tx */
    outw(TxEnable, BASE + MACCtrl1);

/*	if(1){
		int txboguscnt = 32;
		int tx_status = inw(BASE + TxStatus);
		while (tx_status & 0x80) {
			if(tx_status & 0x1e) {
				printf("Transmit Errors\n");
      				if(tx_status & 0x10) {*//* Reset the Tx */
/*					outl(0x001c0000 | inl(BASE + ASICCtrl),
							BASE + ASICCtrl);
					outl(virt_to_bus(&tx_ring[0]), 
							BASE + TxListPtr);
				}
      				if(tx_status & 0x1e) *//* Restart the Tx */
/*					outw(TxEnable, BASE + MACCtrl1);
			}
			outw(0, BASE + TxStatus);	
			if(--txboguscnt <0)
				break;
			tx_status = inw(BASE + TxStatus);
		}
	}
*/


    to = currticks() + TX_TIME_OUT;
    while ((tx_ring[0].status & 0x8000) && (currticks() < to))
	/* wait */ ;
    outw(0, BASE + TxStatus);
    if (currticks() >= to) {
	printf("TX Time Out");
    }
    outw(TxDisable, BASE + MACCtrl1);

    /* send the packet to destination */
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void sundance_disable(struct dev *dev)
{
    /* put the card in its initial state */
    /* This function serves 3 purposes.
     * This disables DMA and interrupts so we don't receive
     *  unexpected packets or interrupts from the card after
     *  etherboot has finished.
     * This frees resources so etherboot may use
     *  this driver on another interface
     * This allows etherboot to reinitialize the interface
     *  if something is something goes wrong.
     */
    sundance_reset((struct nic *) dev);

    /* Disable the interrupts */
    outw(0x0000, BASE + IntrEnable);

    /* Stop the Chipchips Tx and Rx Status */
    outw(TxDisable | RxDisable | StatsDisable, BASE + MACCtrl1);


}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/
static int sundance_probe(struct dev *dev, struct pci_device *pci)
{
    struct nic *nic = (struct nic *) dev;
    int card_idx = 1;
    int i, option = card_idx < MAX_UNITS ? options[card_idx] : 0;

    if (pci->ioaddr == 0)
	return 0;

    /* BASE is used throughout to address the card */
    BASE = pci->ioaddr;
    printf("Looking for Sundance Chip: Vendor=%hX   Device=%hX\n",
	   pci->dev_id, pci->name);

    /* Get the MAC Address by reading the EEPROM */
    for (i = 0; i < 3; i++) {
	((u16 *) ee_data)[i] =
	    le16_to_cpu(eeprom_read(BASE, i + EEPROM_SA_OFFSET));
    }
    /* Update the nic structure with the MAC Address */
    for (i = 0; i < ETH_ALEN; i++) {
	nic->node_addr[i] = ee_data[i];
    }
    /* Print out some hardware info */
    printf("%! at ioaddr %hX\n", nic->node_addr, BASE);

    /* I really must find out what this does */
    adjust_pci_device(pci);

    /* point to private storage */
    sdc = &sdx;

    sdc->chip_id = 0;		/* Undefined */
    sdc->drv_flags = 0;		/* Undefined */
    sdc->rx_copybreak = rx_copybreak;
    sdc->max_interrupt_word = max_interrupt_work;
    sdc->multicast_filter_limit = multicast_filter_limit;
    sdc->vendor_id = pci->vendor;
    sdc->dev_id = pci->dev_id;
    sdc->nic_name = pci->name;

    /* Useful ? */
    sdc->if_port = 0;
    sdc->default_port = 0;

    if (card_idx < MAX_UNITS && full_duplex[card_idx] > 0)
	sdc->full_duplex = 1;

    if (sdc->full_duplex)
	sdc->medialock = 1;

    if (1) {
	int phy, phy_idx = 0;
	sdc->phys[0] = 1;	/* Default Setting */
	mii_preamble_required++;
	for (phy = 1; phy < 32 && phy_idx < 4; phy++) {
	    int mii_status = mdio_read(nic, phy, 1);
	    if (mii_status != 0xffff && mii_status != 0x0000) {
		sdc->phys[phy_idx++] = phy;
		sdc->advertizing = mdio_read(nic, phy, 4);
		if ((mii_status & 0x0040) == 0)
		    mii_preamble_required++;
		printf("%s: MII PHY found at address %d, "
		       "status %hX advertizing %hX\n",
		       sdc->nic_name, phy, mii_status, sdc->advertizing);
	    }
	}
	mii_preamble_required--;
	sdc->mii_cnt = phy_idx;
	if (phy_idx == 0)
	    printf("%s: No MII transceiver found!\n", sdc->nic_name);
    }

    /* Allow forcing the media type */
    if (option > 0) {
	if (option & 0x220)
	    sdc->full_duplex = 1;
	sdc->default_port = option & 0x3ff;
	if (sdc->default_port & 0x330) {
	    sdc->medialock = 1;
	    printf("%s: Forcing %dMbs %s-duplex operation.\n",
		   (option & 0x300 ? 100 : 10),
		   (sdc->full_duplex ? "full" : "half"));
	    if (sdc->mii_cnt)
		mdio_write(nic, sdc->phys[0], 0,
			   ((option & 0x300) ? 0x2000 : 0) |
			   (sdc->full_duplex ? 0x0100 : 0));
	}
    }

    /* Reset the chip to erase previous misconfiguration */
    /* FIXME: Set the printf for debugging only */
    printf("ASIC Control is %hX\n", (int) inl(BASE + ASICCtrl));
    outl(0x007f0000, BASE + ASICCtrl);
    printf("ASIC Control is now %hX\n", (int) inl(BASE + ASICCtrl));

    sundance_reset(nic);

    /* point to NIC specific routines */
    dev->disable = sundance_disable;
    nic->poll = sundance_poll;
    nic->transmit = sundance_transmit;

    /* Give card a chance to reset before returning */
    udelay(100);

    return 1;
}



static int eeprom_read(long ioaddr, int location)
{
    int boguscnt = 2000;	/* Typical 190 ticks */
    outw(0x0200 | (location & 0xff), ioaddr + EECtrl);
    do {
	if (!(inw(ioaddr + EECtrl) & 0x8000)) {
	    return inw(ioaddr + EEData);
	}
    }
    while (--boguscnt > 0);
    return 0;
}

/* MII transceiver section */
#define mdio_delay() inb(mdio_addr)


#define MDIO_EnbIn  (0)
#define MDIO_WRITE0  (MDIO_EnbOutput)
#define MDIO_WRITE1  (MDIO_Data | MDIO_EnbOutput)

enum mii_reg_bits {
    MDIO_ShiftClk = 0x0001, MDIO_Data = 0x0002, MDIO_EnbOutput = 0x0004,
};

/* Generate the preamble required for initial synchronization and a few older tranceivers */
static void mdio_sync(long mdio_addr)
{
    int bits = 32;

    /* Establish sync by sending at least 32 logic ones */
    while (--bits >= 0) {
	outb(MDIO_WRITE1, mdio_addr);
	mdio_delay();
	outb(MDIO_WRITE1 | MDIO_ShiftClk, mdio_addr);
	mdio_delay();
    }
}

static int mdio_read(struct nic *nic, int phy_id, int location)
{
    long mdio_addr = BASE + MIICtrl;
    int mii_cmd = (0xf6 << 10) | (phy_id << 5) | location;
    int i, retval = 0;

    /* Assume that sync is always required */
    mdio_sync(mdio_addr);

    /* Shift the read command bits out */
    for (i = 15; i >= 0; i--) {
	int dataval = (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;
	outb(dataval, mdio_addr);
	mdio_delay();
	outb(dataval | MDIO_ShiftClk, mdio_addr);
	mdio_delay();
    }
    /* Read the two transition 16 data and wire-idel bits. */
    for (i = 19; i > 0; i--) {
	outb(MDIO_EnbIn, mdio_addr);
	mdio_delay();
	retval = (retval << 1) | ((inb(mdio_addr) & MDIO_Data) ? 1 : 0);
	outb(MDIO_EnbIn | MDIO_ShiftClk, mdio_addr);
	mdio_delay();
    }
    return (retval >> 1) & 0xffff;
}

static void
mdio_write(struct nic *nic, int phy_id, int location, int value)
{
    long mdio_addr = BASE + MIICtrl;
    int mii_cmd =
	(0x5002 << 16) | (phy_id << 23) | (location << 18) | value;
    int i;

    if (mii_preamble_required)
	mdio_sync(mdio_addr);

    /* Shift the bytes out */
    for (i = 31; i >= 0; i--) {
	int dataval = (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;
	outb(dataval, mdio_addr);
	mdio_delay();
	outb(dataval | MDIO_ShiftClk, mdio_addr);
	mdio_delay();
    }

    /* Clear the extra bits */
    for (i = 2; i > 0; i--) {
	outb(MDIO_EnbIn, mdio_addr);
	mdio_delay();
	outb(MDIO_EnbIn | MDIO_ShiftClk, mdio_addr);
	mdio_delay();
    }
    return;
}

static void set_rx_mode(struct nic *nic)
{
/*This could be filled out for Multicast and Promiscuous */
    outb((AcceptMyPhys | AcceptBroadcast), BASE + RxMode);
    return;

}

static struct pci_id sundance_nics[] = {
    {PCI_VENDOR_ID_SUNDANCE, PCI_DEVICE_ID_SUNDANCE_ALTA,
     "S201 Sundance 'Alta' based Adaptor"},
    {PCI_VENDOR_ID_DLINK, PCI_DEVICE_ID_DFE530TXS,
     "D-Link DFE530TXS (Sundance ST201 Alta)"},
};

static struct pci_driver sundance_driver __pci_driver = {
    .type = NIC_DRIVER,
    .name = "SUNDANCE/PCI",
    .probe = sundance_probe,
    .ids = sundance_nics,
    .id_count = sizeof(sundance_nics) / sizeof(sundance_nics[0]),
};