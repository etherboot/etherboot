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

#define EDEBUG
/* Set the mtu */
static int mtu = 1514;

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;

/* Maximum number of multicast addresses to filter (vs. rx-all-multicast).
   The sundance uses a 64 element hash table based on the Ethernet CRC.  */
static int multicast_filter_limit = 32;

/* Set the copy breakpoint for the copy-only-tiny-frames scheme.
   Setting to > 1518 effectively disables this feature.
   This chip can receive into any byte alignment buffers, so word-oriented
   archs do not need a copy-align of the IP header. */
static int rx_copybreak = 0;

/* Used to pass the media type, etc.
   Both 'options[]' and 'full_duplex[]' should exist for driver
   interoperability.
   The media type is usually passed in 'options[]'.
    The default is autonegotation for speed and duplex.
	This should rarely be overridden.
    Use option values 0x10/0x20 for 10Mbps, 0x100,0x200 for 100Mbps.
    Use option values 0x10 and 0x100 for forcing half duplex fixed speed.
    Use option values 0x20 and 0x200 for forcing full duplex operation.
*/
#define MAX_UNITS 8
static int options[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int full_duplex[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

/* Condensed operations for readability. */
#define virt_to_le32desc(addr)  cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr)  bus_to_virt(le32_to_cpu(addr))

/* Operational parameters that are set at compile time. */

/* Ring sizes are a power of two only for compile efficiency.
   The compiler will convert <unsigned>'%'<2^N> into a bit mask.
   There must be at least five Tx entries for the tx_full hysteresis, and
   more than 31 requires modifying the Tx status handling error recovery.
   Leave a inactive gap in the Tx ring for better cache behavior.
   Making the Tx ring too large decreases the effectiveness of channel
   bonding and packet priority.
   Large receive rings waste memory and impact buffer accounting.
   The driver need to protect against interrupt latency and the kernel
   not reserving enough available memory.
*/
#define TX_RING_SIZE	2
#define TX_QUEUE_LEN	10	/* Limit ring entries actually used.  */
#define RX_RING_SIZE	4


/* Operational parameters that usually are not changed. */
/* Time in jiffies before concluding the transmitter is hung. */
#define HZ 100
#define TX_TIME_OUT	  (6*HZ)

/* Allocation size of Rx buffers with normal sized Ethernet frames.
   Do not change this value without good reason.  This is not a limit,
   but a way to keep a consistent allocation size among drivers.
 */
#define PKT_BUF_SZ	1536

/* Set iff a MII transceiver on any interface requires mdio preamble.
   This only set with older tranceivers, so the extra
   code size of a per-interface flag is not worthwhile. */
static int mii_preamble_required = 0;

/* Offsets to the device registers.
   Unlike software-only systems, device drivers interact with complex hardware.
   It's not useful to define symbolic names for every register bit in the
   device.  The name can only partially document the semantics and make
   the driver longer and more difficult to read.
   In general, only the important configuration values or bits changed
   multiple times should be defined symbolically.
*/
enum alta_offsets {
	DMACtrl = 0x00, TxListPtr = 0x04, TxDMACtrl = 0x08, TxDescPoll =
	    0x0a,
	RxDMAStatus = 0x0c, RxListPtr = 0x10, RxDMACtrl =
	    0x14, RxDescPoll = 0x16,
	LEDCtrl = 0x1a, ASICCtrl = 0x30,
	EEData = 0x34, EECtrl = 0x36, TxThreshold = 0x3c,
	FlashAddr = 0x40, FlashData = 0x44, WakeEvent = 0x45, TxStatus =
	    0x46,
	DownCounter = 0x48, IntrClear = 0x4a, IntrEnable =
	    0x4c, IntrStatus = 0x4e,
	MACCtrl0 = 0x50, MACCtrl1 = 0x52, StationAddr = 0x54,
	MaxFrameSize = 0x5A, RxMode = 0x5c, MIICtrl = 0x5e,
	MulticastFilter0 = 0x60, MulticastFilter1 = 0x64,
	RxOctetsLow = 0x68, RxOctetsHigh = 0x6a, TxOctetsLow =
	    0x6c, TxOctetsHigh = 0x6e,
	TxFramesOK = 0x70, RxFramesOK = 0x72, StatsCarrierError = 0x74,
	StatsLateColl = 0x75, StatsMultiColl = 0x76, StatsOneColl = 0x77,
	StatsTxDefer = 0x78, RxMissed = 0x79, StatsTxXSDefer =
	    0x7a, StatsTxAbort = 0x7b,
	StatsBcastTx = 0x7c, StatsBcastRx = 0x7d, StatsMcastTx =
	    0x7e, StatsMcastRx = 0x7f,
	/* Aliased and bogus values! */
	RxStatus = 0x0c,
};

/* Bits in the interrupt status/mask registers. */
enum intr_status_bits {
	IntrSummary = 0x0001, IntrPCIErr = 0x0002, IntrMACCtrl = 0x0008,
	IntrTxDone = 0x0004, IntrRxDone = 0x0010, IntrRxStart = 0x0020,
	IntrDrvRqst = 0x0040,
	StatsMax = 0x0080, LinkChange = 0x0100,
	IntrTxDMADone = 0x0200, IntrRxDMADone = 0x0400,
};

/* Bits in the RxMode register. */
enum rx_mode_bits {
	AcceptAllIPMulti = 0x20, AcceptMultiHash = 0x10, AcceptAll = 0x08,
	AcceptBroadcast = 0x04, AcceptMulticast = 0x02, AcceptMyPhys =
	    0x01,
};
/* Bits in MACCtrl. */
enum mac_ctrl0_bits {
	EnbFullDuplex = 0x20, EnbRcvLargeFrame = 0x40,
	EnbFlowCtrl = 0x100, EnbPassRxCRC = 0x200,
};
enum mac_ctrl1_bits {
	StatsEnable = 0x0020, StatsDisable = 0x0040, StatsEnabled = 0x0080,
	TxEnable = 0x0100, TxDisable = 0x0200, TxEnabled = 0x0400,
	RxEnable = 0x0800, RxDisable = 0x1000, RxEnabled = 0x2000,
};

/* The Rx and Tx buffer descriptors.
   Using only 32 bit fields simplifies software endian correction.
   This structure must be aligned, and should avoid spanning cache lines.
*/
struct netdev_desc {
	u32 next_desc;
	u32 status;
	u32 addr;
	u32 length;
};

/* Bits in netdev_desc.status */
enum desc_status_bits {
	DescOwn = 0x8000, DescEndPacket = 0x4000, DescEndRing = 0x2000,
	DescTxDMADone = 0x10000,
	LastFrag = 0x80000000, DescIntrOnTx = 0x8000, DescIntrOnDMADone =
	    0x80000000,
};


/* Define the TX Descriptor */
static struct netdev_desc tx_ring[TX_RING_SIZE]
    __attribute__ ((aligned(8)));

/* Create a static buffer of size PKT_BUF_SZ for each
TX Descriptor.  All descriptors point to a
part of this buffer */
static unsigned char txb[PKT_BUF_SZ * TX_RING_SIZE]
    __attribute__ ((aligned(8)));


/* Define the RX Descriptor */
static struct netdev_desc rx_ring[RX_RING_SIZE]
    __attribute__ ((aligned(8)));

/* Create a static buffer of size PKT_BUF_SZ for each
RX Descriptor   All descriptors point to a
part of this buffer */
static unsigned char rxb[RX_RING_SIZE * PKT_BUF_SZ]
    __attribute__ ((aligned(8)));


static u32 BASE;
#define EEPROM_SIZE	128

enum pci_id_flags_bits {
	PCI_USES_IO = 1, PCI_USES_MEM = 2, PCI_USES_MASTER = 4,
	PCI_ADDR0 = 0 << 4, PCI_ADDR1 = 1 << 4, PCI_ADDR2 =
	    2 << 4, PCI_ADDR3 = 3 << 4,
};

enum chip_capability_flags { CanHaveMII = 1, KendinPktDropBug = 2, };
#define PCI_IOTYPE (PCI_USES_MASTER | PCI_USES_IO  | PCI_ADDR0)

struct pci_id_info {
	char *name;
	struct match_info {
		u32 pci, pci_mask, subsystem, subsystem_mask;
		u32 revision, revision_mask;	/* Only 8 bits. */
	} id;
	enum pci_id_flags_bits pci_flags;
	int io_size;		/* Needed for I/O region check or ioremap(). */
	int drv_flags;		/* Driver use, intended as capability flags. */
};

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
	unsigned rx_buf_sz;	/* Based on mtu + Slack */
	int rx_copybreak;

	struct netdev_desc *rx_head_desc;
	unsigned int cur_tx, dirty_tx;
	unsigned int tx_full:1;
	unsigned int mtu;

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
	u16 advertizing;	/* NWay media advertizing */
	unsigned char phys[2];
	int budget;

	int saved_if_port;
} sdx;

static struct sundance_private *sdc;

/* Station Address location within the EEPROM */
#define EEPROM_SA_OFFSET	0x10



static int eeprom_read(long ioaddr, int location);
static int mdio_read(struct nic *nic, int phy_id, unsigned int location);
static void mdio_write(struct nic *nic, int phy_id, unsigned int location,
		       int value);
static void set_rx_mode(struct nic *nic);
static void refill_rx(struct nic *nic);

static void check_duplex(struct nic *nic)
{
	int mii_reg5 = mdio_read(nic, sdc->phys[0], 5);
	int negotiated = mii_reg5 & sdc->advertizing;
	int duplex;

	if (sdc->duplex_lock || mii_reg5 == 0xffff)
		return;
	duplex = (negotiated & 0x0100) || (negotiated & 0x01C0) == 0x0040;
	if (sdc->full_duplex != duplex) {
		sdc->full_duplex = duplex;
		printf("%s: Setting %s-duplex based on MII #%d "
		       "negociated capability %hX\n", sdc->nic_name,
		       duplex ? "full" : "half", sdc->phys[0], negotiated);
		outw(duplex ? 0x20 : 0, BASE + MACCtrl0);

	}
}


/**************************************************************************
 *  init_ring - setup the tx and rx descriptors
 *************************************************************************/
static void init_ring(struct nic *nic __unused)
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
		rx_ring[i].addr = virt_to_le32desc(&rxb[i * PKT_BUF_SZ]);
		rx_ring[i].length = cpu_to_le32(sdc->rx_buf_sz | LastFrag);
	}
	sdc->dirty_rx = (unsigned int) (i - RX_RING_SIZE);

	/* We only use one transmit buffer, but two 
	 * descriptors so transmit engines have somewhere 
	 * to point should they feel the need */
	tx_ring[0].status = 0x00000000;
	tx_ring[0].addr = virt_to_bus(&txb[0]);
	tx_ring[0].next_desc = 0; /* virt_to_bus(&tx_ring[1]); */

	/* This descriptor is never used */
	tx_ring[1].status = 0x00000000;
	tx_ring[1].addr = 0; /*virt_to_bus(&txb[0]); */
	tx_ring[1].next_desc = 0; 

	/* Mark the last entry as wrapping the ring, 
	 * though this should never happen */
	tx_ring[1].length = cpu_to_le32(LastFrag | PKT_BUF_SZ);
}

/**************************************************************************
 *  RESET - Reset Adapter
 * ***********************************************************************/
static void sundance_reset(struct nic *nic)
{
	int i;

	init_ring(nic);

	/* FIXME: find out where the linux driver sets duplex_lock */
	sdc->full_duplex = sdc->duplex_lock;

	/* The Tx List Pointer is written as packets are queued */
	outl(virt_to_le32desc(&rx_ring[0]), BASE + RxListPtr);

	/* Write the MAC address to the StationAddress */
	for (i = 0; i < 6; i++)
		outb(nic->node_addr[i], BASE + StationAddr + i);

	/* Get the link status */
	sdc->link_status = inb(BASE + MIICtrl) & 0xE0;
	/* Write the status to the MACCtrl0 register */
	outw((sdc->full_duplex || (sdc->link_status & 0x20)) ? 0x120 : 0,
	     BASE + MACCtrl0);
	outw(sdc->mtu + 14, BASE + MaxFrameSize);
	if (sdc->mtu > 2047)/* this will never happen with default options */
		outl(inl(BASE + ASICCtrl) | 0x0c, BASE + ASICCtrl);

	set_rx_mode(nic);
	outw(0, BASE + DownCounter);
	/* Set the chip to poll every N*30nsec */
	outb(100, BASE + RxDescPoll);
/*	outb(127, BASE + TxDescPoll); */

/* FIXME: Linux Driver has a bug fix for kendin nic */

/* FIXME: Do we really need stats enabled?*/
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
#ifdef EDEBUG
	printf("%s: Done sundance_reset, status: Rx %hX Tx %hX "
	       "MAC Control %hX, %hX %hX\n",
	       sdc->nic_name, (int) inl(BASE + RxStatus),
	       (int) inw(BASE + TxStatus), (int) inl(BASE + MACCtrl0),
	       (int) inw(BASE + MACCtrl1), (int) inw(BASE + MACCtrl0));
#endif
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int sundance_poll(struct nic *nic)
{

	/* FIXME: The entire POLL procedure needs to be cleand up */

	/* return true if there's an ethernet packet ready to read */
	/* nic->packet should contain data on return */
	/* nic->packetlen should contain length of data */
	int entry = sdc->cur_rx % RX_RING_SIZE;
	int boguscnt = 32;
	int received = 0;
	struct netdev_desc *desc = &(rx_ring[entry]);
	u32 frame_status = le32_to_cpu(desc->status);
	int pkt_len = 0;

	if (--boguscnt < 0) {
		goto not_done;
	}
	if (!(frame_status & DescOwn))
		return 0;
	pkt_len = frame_status & 0x1fff;
	if (frame_status & 0x001f4000) {
#ifdef EDEBUG
		/* FIXME: Do we really care about this */
		printf("There was an error\n");
#endif
	} else {
		if (pkt_len < rx_copybreak) {
			/* FIXME: What should happen Will this ever occur */
			printf("Problem");
		} else {
			nic->packetlen = pkt_len;
			memcpy(nic->packet, rxb +
			       (sdc->cur_rx * PKT_BUF_SZ), nic->packetlen);

		}
	}
	entry = (entry + 1) % RX_RING_SIZE;
	received++;
	sdc->cur_rx = entry;
	refill_rx(nic);
	sdc->budget -= received;
	return 1;


      not_done:
	sdc->cur_rx = entry;
	refill_rx(nic);
	if (!received)
		received = 1;
	sdc->budget -= received;
	if (sdc->budget <= 0)
		sdc->budget = 32;
#ifdef EDEBUG
	printf("Not Done\n");
#endif
	return 0;

}
static void refill_rx(struct nic *nic __unused)
{
	int entry;
	int cnt = 0;

	/* Refill the Rx ring buffers. */
	for (;
	     (sdc->cur_rx - sdc->dirty_rx + RX_RING_SIZE) % RX_RING_SIZE >
	     0; sdc->dirty_rx = (sdc->dirty_rx + 1) % RX_RING_SIZE) {
		entry = sdc->dirty_rx % RX_RING_SIZE;
		/* Perhaps we need not reset this field. */
		rx_ring[entry].length =
		    cpu_to_le32(sdc->rx_buf_sz | LastFrag);
		rx_ring[entry].status = 0;
		cnt++;
	}
	return;
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
	tx_ring[0].status = cpu_to_le32(0x00000001);

	/* Point to transmit descriptor */
	outl(virt_to_le32desc(&tx_ring[0]), BASE + TxListPtr);

	/* Enable Tx */
	outw(TxEnable, BASE + MACCtrl1);
	outw(0, BASE + TxStatus);
	to = currticks() + TX_TIME_OUT;
	while(!(tx_ring[0].status & 0x00010000) &&  (currticks() < to))
		; /* wait */ 

	if (currticks() >= to) {
		printf("TX Time Out");
	}
	/* Disable Tx */
	outw(TxDisable, BASE + MACCtrl1);
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
	u8 ee_data[EEPROM_SIZE];
	int i, option = card_idx < MAX_UNITS ? options[card_idx] : 0;

	if (pci->ioaddr == 0)
		return 0;

	/* BASE is used throughout to address the card */
	BASE = pci->ioaddr;
	printf("%s: Probing for Vendor=%hX   Device=%hX, %s\n",
	       pci->name, pci->vendor, pci->dev_id);

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
	printf("%s: %! at ioaddr %hX\n", pci->name, nic->node_addr, BASE);

	/* I really must find out what this does */
	adjust_pci_device(pci);

	/* point to private storage */
	sdc = &sdx;

	sdc->chip_id = 0;	/* Undefined */
	sdc->drv_flags = 0;	/* Undefined */
	sdc->rx_copybreak = rx_copybreak;
	sdc->max_interrupt_word = max_interrupt_work;
	sdc->multicast_filter_limit = multicast_filter_limit;
	sdc->vendor_id = pci->vendor;
	sdc->dev_id = pci->dev_id;
	sdc->nic_name = pci->name;
	sdc->mtu = mtu;

	if (card_idx < MAX_UNITS && full_duplex[card_idx] > 0) {
		sdc->full_duplex = 1;
#ifdef EDEBUG
		printf("sdc->full_duplex = 1\n");
#endif
	}
	if (sdc->full_duplex) {
		sdc->medialock = 1;
#ifdef EDEBUG
		printf("sdc->media_lock = 1\n");
#endif
	}

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
#ifdef EDEBUG
				printf
				    ("%s: MII PHY found at address %d, status "
				     "%hX advertizing %hX\n",
				     sdc->nic_name, phy, mii_status,
				     sdc->advertizing);
#endif
			}
		}
		mii_preamble_required--;
		sdc->mii_cnt = phy_idx;
		if (phy_idx == 0)
			printf("%s: No MII transceiver found!\n",
			       sdc->nic_name);
	}

	/* Allow forcing the media type */
	if (option > 0) {
#ifdef EDEBUG
		printf("Trying to force the media type\n");
#endif
		if (option & 0x220)
			sdc->full_duplex = 1;
		sdc->default_port = option & 0x3ff;
		if (sdc->default_port & 0x330) {
			sdc->medialock = 1;
#ifdef EDEBUG
			printf("Forcing %dMbs %s-duplex operation.\n",
			       (option & 0x300 ? 100 : 10),
			       (sdc->full_duplex ? "full" : "half"));
#endif
			if (sdc->mii_cnt)
				mdio_write(nic, sdc->phys[0],
					0, ((option & 0x300) ? 0x2000 : 0)
					|	/* 100mbps */
					(sdc->full_duplex ? 0x0100 : 0));	/* Full Duplex? */
		}
	}

	/* Reset the chip to erase previous misconfiguration */
#ifdef EDEBUG
	printf("ASIC Control is %hX\n", (int) inl(BASE + ASICCtrl));
#endif
	outl(0x007f0000 | inl(BASE + ASICCtrl), BASE + ASICCtrl);
#ifdef EDEBUG
	printf("ASIC Control is now %hX\n", (int) inl(BASE + ASICCtrl));
#endif

	sundance_reset(nic);

	/* point to NIC specific routines */
	dev->disable = sundance_disable;
	nic->poll = sundance_poll;
	nic->transmit = sundance_transmit;

	return 1;
}


/* Read the EEPROM and MII Management Data I/O (MDIO) interfaces. */
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

/*  MII transceiver control section.
	Read and write the MII registers using software-generated serial
	MDIO protocol.  See the MII specifications or DP83840A data sheet
	for details.

	The maximum data clock rate is 2.5 Mhz.
	The timing is decoupled from the processor clock by flushing the write
	from the CPU write buffer with a following read, and using PCI
	transaction time. */

#define mdio_in(mdio_addr) inb(mdio_addr)
#define mdio_out(value, mdio_addr) outb(value, mdio_addr)
#define mdio_delay(mdio_addr) inb(mdio_addr)

enum mii_reg_bits {
	MDIO_ShiftClk = 0x0001, MDIO_Data = 0x0002, MDIO_EnbOutput =
	    0x0004,
};
#define MDIO_EnbIn  (0)
#define MDIO_WRITE0 (MDIO_EnbOutput)
#define MDIO_WRITE1 (MDIO_Data | MDIO_EnbOutput)

/* Generate the preamble required for initial synchronization and
   a few older transceivers. */
static void mdio_sync(long mdio_addr)
{
	int bits = 32;

	/* Establish sync by sending at least 32 logic ones. */
	while (--bits >= 0) {
		mdio_out(MDIO_WRITE1, mdio_addr);
		mdio_delay(mdio_addr);
		mdio_out(MDIO_WRITE1 | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
}

static int
mdio_read(struct nic *nic __unused, int phy_id, unsigned int location)
{
	long mdio_addr = BASE + MIICtrl;
	int mii_cmd = (0xf6 << 10) | (phy_id << 5) | location;
	int i, retval = 0;

	if (mii_preamble_required)
		mdio_sync(mdio_addr);

	/* Shift the read command bits out. */
	for (i = 15; i >= 0; i--) {
		int dataval =
		    (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;

		mdio_out(dataval, mdio_addr);
		mdio_delay(mdio_addr);
		mdio_out(dataval | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
	/* Read the two transition, 16 data, and wire-idle bits. */
	for (i = 19; i > 0; i--) {
		mdio_out(MDIO_EnbIn, mdio_addr);
		mdio_delay(mdio_addr);
		retval = (retval << 1) | ((mdio_in(mdio_addr) & MDIO_Data)
					  ? 1 : 0);
		mdio_out(MDIO_EnbIn | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
	return (retval >> 1) & 0xffff;
}

static void
mdio_write(struct nic *nic __unused, int phy_id,
	   unsigned int location, int value)
{
	long mdio_addr = BASE + MIICtrl;
	int mii_cmd =
	    (0x5002 << 16) | (phy_id << 23) | (location << 18) | value;
	int i;

	if (mii_preamble_required)
		mdio_sync(mdio_addr);

	/* Shift the command bits out. */
	for (i = 31; i >= 0; i--) {
		int dataval =
		    (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;
		mdio_out(dataval, mdio_addr);
		mdio_delay(mdio_addr);
		mdio_out(dataval | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
	/* Clear out extra bits. */
	for (i = 2; i > 0; i--) {
		mdio_out(MDIO_EnbIn, mdio_addr);
		mdio_delay(mdio_addr);
		mdio_out(MDIO_EnbIn | MDIO_ShiftClk, mdio_addr);
		mdio_delay(mdio_addr);
	}
	return;
}

/* The little-endian AUTODIN II ethernet CRC calculations.
   A big-endian version is also available.
   This is slow but compact code.  Do not use this routine for bulk data,
   use a table-based routine instead.
   This is common code and should be moved to net/core/crc.c.
   Chips may use the upper or lower CRC bits, and may reverse and/or invert
   them.  Select the endian-ness that results in minimal calculations.
*/
static unsigned const ethernet_polynomial_le = 0xedb88320U;
static inline unsigned ether_crc_le(int length, unsigned char *data)
{
	unsigned int crc = ~0;	/* Initial value. */
	while (--length >= 0) {
		unsigned char current_octet = *data++;
		int bit;
		for (bit = 8; --bit >= 0; current_octet >>= 1) {
			if ((crc ^ current_octet) & 1) {
				crc >>= 1;
				crc ^= ethernet_polynomial_le;
			} else
				crc >>= 1;
		}
	}
	return crc;
}

static void set_rx_mode(struct nic *nic __unused)
{
	/* FIXME: Add multicast support */
	outb((AcceptBroadcast | AcceptMyPhys), BASE + RxMode);
	return;
}

static struct pci_id sundance_nics[] = {
PCI_ROM(0x13f0, 0x0201, "sundance",  "ST201 Sundance 'Alta' based Adaptor"),
PCI_ROM(0x1186, 0x1002, "dfe530txs", "D-Link DFE530TXS (Sundance ST201 Alta)"),
};

static struct pci_driver sundance_driver __pci_driver = {
	.type = NIC_DRIVER,
	.name = "SUNDANCE/PCI",
	.probe = sundance_probe,
	.ids = sundance_nics,
	.id_count = sizeof(sundance_nics) / sizeof(sundance_nics[0]),
	.class = 0,
};
