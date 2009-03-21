/**
 *    r8169.c: Etherboot device driver for the RealTek RTL-8169 Gigabit
 *
 *    Copyright (c) 2009 Marty Connor <mdc@etherboot.org>
 *    Copyright (c) 2009 Entity Cyber, Inc.
 *
 *    In March 2009, Marty Connor, <mdc@etherboot.org> did a major
 *    rewrite of this driver to support more variants, based on Linux
 *    Kernel source code.
 *
 *    Funding for this driver rewrite was provided by Sicom Systems:
 *   
 *        http://www.sicom.com/
 *
 *    This driver is based on rtl8169 data sheets and work by:
 *
 *    Copyright (c) 2002 ShuChen <shuchen@realtek.com.tw>
 *    Copyright (c) 2003 - 2007 Francois Romieu <romieu@fr.zoreil.com>
 *    Copyright (c) a lot of people too. Please respect their work.
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
 *    Previous attributions:
 *
 *    Written 2003 by Timothy Legge <tlegge@rogers.com>
 *
 *    Portions of this code based on:
 *	  r8169.c: A RealTek RTL-8169 Gigabit Ethernet driver 
 * 		   for Linux kernel 2.4.x.
 *
 *    Written 2002 ShuChen <shuchen@realtek.com.tw>
 *	  See Linux Driver for full information
 *	
 *    Linux Driver Versions: 
 *	  1.27a, 10.02.2002
 *	  RTL8169_VERSION "2.2"   <2004/08/09>
 * 
 *    Thanks to:
 *    	  Jean Chen of RealTek Semiconductor Corp. for
 *    	  providing the evaluation NIC used to develop 
 *    	  this driver.  RealTek's support for Etherboot 
 *    	  is appreciated.
 *    	
 *    REVISION HISTORY:
 *    ================
 *
 *    v2.0      03-16-2009	mdc		Major rewrite using Linux driver as
 *						as base.
 *    v1.7	11-22-2005	timlegge	Update to RealTek Driver Version 2.2
 *    v1.6	03-27-2004	timlegge	Additional Cleanup
 *    v1.5	01-17-2004	timlegge	Initial driver output cleanup
 *    v1.0	11-26-2003	timlegge	Initial port of Linux driver
 *    
 *    Indent Options: indent -kr -i8
 **/

#include "etherboot.h"
#include "nic.h"
#include "pci.h"
#include "timer.h"

#if 0
#define DBG_R8169
#else
#undef DBG_R8169
#undef DBG2_R8169
#undef DBGP_R8169
#endif

#ifdef DBG_R8169
#define DBG(...) printf ( __VA_ARGS__ )
#else
#define DBG(...)
#endif

#ifdef DBG2_R8169
#define DBG2(...) printf ( __VA_ARGS__ )
#else
#define DBG2(...)
#endif

#ifdef DBGP_R8169
#define DBGP(...) printf ( __VA_ARGS__ )
#else
#define DBGP(...)
#endif

#define HZ 1000
#define TX_TIMEOUT  (6*HZ)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define virt_to_le32desc(addr)  cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr)  bus_to_virt(le32_to_cpu(addr))

#define  PCI_EXP_DEVCTL	        8	/* Device Control */
#define  PCI_EXP_DEVCTL_READRQ	0x7000	/* Max_Read_Request_Size */
#define  PCI_EXP_LNKCTL		16	/* Link Control */
#define  PCI_EXP_LNKCTL_CLKREQ_EN 0x100	/* Enable clkreq */
#define  PCI_EXP_DEVCTL_NOSNOOP_EN 0x0800  /* Enable No Snoop */
#define  PCI_CACHE_LINE_SIZE    0x0c    /* 8 bits */

#ifndef PCI_CAP_ID_EXP
#define  PCI_CAP_ID_EXP 0x10
#endif

/* The forced speed, 10Mb, 100Mb, gigabit, 2.5Gb, 10GbE. */
#define SPEED_10		10
#define SPEED_100		100
#define SPEED_1000		1000
#define SPEED_2500		2500
#define SPEED_10000		10000

/* Duplex, half or full. */
#define DUPLEX_HALF		0x00
#define DUPLEX_FULL		0x01

/* Generic MII registers. */

#define MII_BMCR            0x00        /* Basic mode control register */
#define MII_BMSR            0x01        /* Basic mode status register  */
#define MII_PHYSID1         0x02        /* PHYS ID 1                   */
#define MII_PHYSID2         0x03        /* PHYS ID 2                   */
#define MII_ADVERTISE       0x04        /* Advertisement control reg   */
#define MII_LPA             0x05        /* Link partner ability reg    */
#define MII_EXPANSION       0x06        /* Expansion register          */
#define MII_CTRL1000        0x09        /* 1000BASE-T control          */
#define MII_STAT1000        0x0a        /* 1000BASE-T status           */
#define MII_ESTATUS	    0x0f	/* Extended Status */
#define MII_DCOUNTER        0x12        /* Disconnect counter          */
#define MII_FCSCOUNTER      0x13        /* False carrier counter       */
#define MII_NWAYTEST        0x14        /* N-way auto-neg test reg     */
#define MII_RERRCOUNTER     0x15        /* Receive error counter       */
#define MII_SREVISION       0x16        /* Silicon revision            */
#define MII_RESV1           0x17        /* Reserved...                 */
#define MII_LBRERROR        0x18        /* Lpback, rx, bypass error    */
#define MII_PHYADDR         0x19        /* PHY address                 */
#define MII_RESV2           0x1a        /* Reserved...                 */
#define MII_TPISTATUS       0x1b        /* TPI status for 10mbps       */
#define MII_NCONFIG         0x1c        /* Network interface config    */

/* Basic mode control register. */
#define BMCR_RESV               0x003f  /* Unused...                   */
#define BMCR_SPEED1000		0x0040  /* MSB of Speed (1000)         */
#define BMCR_CTST               0x0080  /* Collision test              */
#define BMCR_FULLDPLX           0x0100  /* Full duplex                 */
#define BMCR_ANRESTART          0x0200  /* Auto negotiation restart    */
#define BMCR_ISOLATE            0x0400  /* Disconnect DP83840 from MII */
#define BMCR_PDOWN              0x0800  /* Powerdown the DP83840       */
#define BMCR_ANENABLE           0x1000  /* Enable auto negotiation     */
#define BMCR_SPEED100           0x2000  /* Select 100Mbps              */
#define BMCR_LOOPBACK           0x4000  /* TXD loopback bits           */
#define BMCR_RESET              0x8000  /* Reset the DP83840           */

/* Basic mode status register. */
#define BMSR_ERCAP              0x0001  /* Ext-reg capability          */
#define BMSR_JCD                0x0002  /* Jabber detected             */
#define BMSR_LSTATUS            0x0004  /* Link status                 */
#define BMSR_ANEGCAPABLE        0x0008  /* Able to do auto-negotiation */
#define BMSR_RFAULT             0x0010  /* Remote fault detected       */
#define BMSR_ANEGCOMPLETE       0x0020  /* Auto-negotiation complete   */
#define BMSR_RESV               0x00c0  /* Unused...                   */
#define BMSR_ESTATEN		0x0100	/* Extended Status in R15 */
#define BMSR_100HALF2           0x0200  /* Can do 100BASE-T2 HDX */
#define BMSR_100FULL2           0x0400  /* Can do 100BASE-T2 FDX */
#define BMSR_10HALF             0x0800  /* Can do 10mbps, half-duplex  */
#define BMSR_10FULL             0x1000  /* Can do 10mbps, full-duplex  */
#define BMSR_100HALF            0x2000  /* Can do 100mbps, half-duplex */
#define BMSR_100FULL            0x4000  /* Can do 100mbps, full-duplex */
#define BMSR_100BASE4           0x8000  /* Can do 100mbps, 4k packets  */

#define AUTONEG_DISABLE		0x00
#define AUTONEG_ENABLE		0x01

#define MII_ADVERTISE           0x04        /* Advertisement control reg   */
#define ADVERTISE_SLCT          0x001f  /* Selector bits               */
#define ADVERTISE_CSMA          0x0001  /* Only selector supported     */
#define ADVERTISE_10HALF        0x0020  /* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL     0x0020  /* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL        0x0040  /* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF     0x0040  /* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF       0x0080  /* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE    0x0080  /* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL       0x0100  /* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM 0x0100  /* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4      0x0200  /* Try for 100mbps 4k packets  */
#define ADVERTISE_PAUSE_CAP     0x0400  /* Try for pause               */
#define ADVERTISE_PAUSE_ASYM    0x0800  /* Try for asymetric pause     */
#define ADVERTISE_RESV          0x1000  /* Unused...                   */
#define ADVERTISE_RFAULT        0x2000  /* Say we can detect faults    */
#define ADVERTISE_LPACK         0x4000  /* Ack link partners response  */
#define ADVERTISE_NPAGE         0x8000  /* Next page bit               */
#define ADVERTISE_FULL (ADVERTISE_100FULL | ADVERTISE_10FULL | \
			ADVERTISE_CSMA)
#define ADVERTISE_ALL (ADVERTISE_10HALF | ADVERTISE_10FULL | \
		       ADVERTISE_100HALF | ADVERTISE_100FULL)

/* 1000BASE-T Control register */
#define ADVERTISE_1000FULL      0x0200  /* Advertise 1000BASE-T full duplex */
#define ADVERTISE_1000HALF      0x0100  /* Advertise 1000BASE-T half duplex */

/* MAC address length */
#define MAC_ADDR_LEN	6

#define MAX_READ_REQUEST_SHIFT	12
#define RX_FIFO_THRESH	7	/* 7 means NO threshold, Rx buffer level before first PCI xfer. */
#define RX_DMA_BURST	6	/* Maximum PCI burst, '6' is 1024 */
#define TX_DMA_BURST	6	/* Maximum PCI burst, '6' is 1024 */
#define EarlyTxThld	0x3F	/* 0x3F means NO early transmit */
#define RxPacketMaxSize	0x3FE8	/* 16K - 1 - ETH_HLEN - VLAN - CRC... */
#define SafeMtu		0x1c20	/* ... actually life sucks beyond ~7k */
#define InterFrameGap	0x03	/* 3 means InterFrameGap = the shortest one */

#define R8169_REGS_SIZE		256
#define R8169_NAPI_WEIGHT	64

#define NUM_TX_DESC	2	/* Number of Tx descriptor registers */
#define NUM_RX_DESC	4	/* Number of Rx descriptor registers */

#define RX_BUF_SIZE	1536	/* Rx Buffer size */
#define TX_BUF_SIZE	1536	/* Rx Buffer size */

#define R8169_TX_RING_BYTES	(NUM_TX_DESC * sizeof(struct TxDesc))
#define R8169_RX_RING_BYTES	(NUM_RX_DESC * sizeof(struct RxDesc))

#define TX_RING_ALIGN		256
#define RX_RING_ALIGN		256

#define RTL8169_TX_TIMEOUT	(6*HZ)
#define RTL8169_PHY_TIMEOUT	(10*HZ)

#define RTL_EEPROM_SIG		cpu_to_le32(0x8129)
#define RTL_EEPROM_SIG_MASK	cpu_to_le32(0xffff)
#define RTL_EEPROM_SIG_ADDR	0x0000

#if 1
/* Use port IO */
#define RTL_W8(reg, val8)   outb ((val8), ioaddr + (reg))
#define RTL_W16(reg, val16) outw ((val16), ioaddr + (reg))
#define RTL_W32(reg, val32) outl ((val32), ioaddr + (reg))
#define RTL_R8(reg)         inb (ioaddr + (reg))
#define RTL_R16(reg)        inw (ioaddr + (reg))
#define RTL_R32(reg)        ((unsigned long) inl (ioaddr + (reg)))
#else
/* Use  MMIO */
#define RTL_W8(reg, val8)	writeb ((val8), ioaddr + (reg))
#define RTL_W16(reg, val16)	writew ((val16), ioaddr + (reg))
#define RTL_W32(reg, val32)	writel ((val32), ioaddr + (reg))
#define RTL_R8(reg)		readb (ioaddr + (reg))
#define RTL_R16(reg)		readw (ioaddr + (reg))
#define RTL_R32(reg)		((unsigned long) readl (ioaddr + (reg)))
#endif

enum mac_version {
	RTL_GIGA_MAC_VER_01 = 0x01, // 8169
	RTL_GIGA_MAC_VER_02 = 0x02, // 8169S
	RTL_GIGA_MAC_VER_03 = 0x03, // 8110S
	RTL_GIGA_MAC_VER_04 = 0x04, // 8169SB
	RTL_GIGA_MAC_VER_05 = 0x05, // 8110SCd
	RTL_GIGA_MAC_VER_06 = 0x06, // 8110SCe
	RTL_GIGA_MAC_VER_07 = 0x07, // 8102e
	RTL_GIGA_MAC_VER_08 = 0x08, // 8102e
	RTL_GIGA_MAC_VER_09 = 0x09, // 8102e
	RTL_GIGA_MAC_VER_10 = 0x0a, // 8101e
	RTL_GIGA_MAC_VER_11 = 0x0b, // 8168Bb
	RTL_GIGA_MAC_VER_12 = 0x0c, // 8168Be
	RTL_GIGA_MAC_VER_13 = 0x0d, // 8101Eb
	RTL_GIGA_MAC_VER_14 = 0x0e, // 8101 ?
	RTL_GIGA_MAC_VER_15 = 0x0f, // 8101 ?
	RTL_GIGA_MAC_VER_16 = 0x11, // 8101Ec
	RTL_GIGA_MAC_VER_17 = 0x10, // 8168Bf
	RTL_GIGA_MAC_VER_18 = 0x12, // 8168CP
	RTL_GIGA_MAC_VER_19 = 0x13, // 8168C
	RTL_GIGA_MAC_VER_20 = 0x14, // 8168C
	RTL_GIGA_MAC_VER_21 = 0x15, // 8168C
	RTL_GIGA_MAC_VER_22 = 0x16, // 8168C
	RTL_GIGA_MAC_VER_23 = 0x17, // 8168CP
	RTL_GIGA_MAC_VER_24 = 0x18, // 8168CP
	RTL_GIGA_MAC_VER_25 = 0x19, // 8168D
};

#define _R(NAME,MAC,MASK) \
	{ .name = NAME, .mac_version = MAC, .RxConfigMask = MASK }

static const struct {
	const char *name;
	u8 mac_version;
	uint32_t RxConfigMask;	/* Clears the bits supported by this chip */
} rtl_chip_info[] = {
	_R("RTL8169",		RTL_GIGA_MAC_VER_01, 0xff7e1880), // 8169
	_R("RTL8169s",		RTL_GIGA_MAC_VER_02, 0xff7e1880), // 8169S
	_R("RTL8110s",		RTL_GIGA_MAC_VER_03, 0xff7e1880), // 8110S
	_R("RTL8169sb/8110sb",	RTL_GIGA_MAC_VER_04, 0xff7e1880), // 8169SB
	_R("RTL8169sc/8110sc",	RTL_GIGA_MAC_VER_05, 0xff7e1880), // 8110SCd
	_R("RTL8169sc/8110sc",	RTL_GIGA_MAC_VER_06, 0xff7e1880), // 8110SCe
	_R("RTL8102e",		RTL_GIGA_MAC_VER_07, 0xff7e1880), // PCI-E
	_R("RTL8102e",		RTL_GIGA_MAC_VER_08, 0xff7e1880), // PCI-E
	_R("RTL8102e",		RTL_GIGA_MAC_VER_09, 0xff7e1880), // PCI-E
	_R("RTL8101e",		RTL_GIGA_MAC_VER_10, 0xff7e1880), // PCI-E
	_R("RTL8168b/8111b",	RTL_GIGA_MAC_VER_11, 0xff7e1880), // PCI-E
	_R("RTL8168b/8111b",	RTL_GIGA_MAC_VER_12, 0xff7e1880), // PCI-E
	_R("RTL8101e",		RTL_GIGA_MAC_VER_13, 0xff7e1880), // PCI-E 8139
	_R("RTL8100e",		RTL_GIGA_MAC_VER_14, 0xff7e1880), // PCI-E 8139
	_R("RTL8100e",		RTL_GIGA_MAC_VER_15, 0xff7e1880), // PCI-E 8139
	_R("RTL8168b/8111b",	RTL_GIGA_MAC_VER_17, 0xff7e1880), // PCI-E
	_R("RTL8101e",		RTL_GIGA_MAC_VER_16, 0xff7e1880), // PCI-E
	_R("RTL8168cp/8111cp",	RTL_GIGA_MAC_VER_18, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_19, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_20, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_21, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_22, 0xff7e1880), // PCI-E
	_R("RTL8168cp/8111cp",	RTL_GIGA_MAC_VER_23, 0xff7e1880), // PCI-E
	_R("RTL8168cp/8111cp",	RTL_GIGA_MAC_VER_24, 0xff7e1880), // PCI-E
	_R("RTL8168d/8111d",	RTL_GIGA_MAC_VER_25, 0xff7e1880)  // PCI-E
};
#undef _R

enum cfg_version {
	RTL_CFG_0 = 0x00,
	RTL_CFG_1,
	RTL_CFG_2
};

#if 0
/** Device Table from Linux Driver **/
static struct pci_device_id rtl8169_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8129), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8136), 0, 0, RTL_CFG_2 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8167), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8168), 0, 0, RTL_CFG_1 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8169), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK,	0x4300), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_AT,		0xc107), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(0x16ec,			0x0116), 0, 0, RTL_CFG_0 },
	{ PCI_VENDOR_ID_LINKSYS,		0x1032,
		PCI_ANY_ID, 0x0024, 0, 0, RTL_CFG_0 },
	{ 0x0001,				0x8168,
		PCI_ANY_ID, 0x2410, 0, 0, RTL_CFG_2 },
	{0,},
};
#endif

enum rtl_registers {
	MAC0		= 0,	/* Ethernet hardware address. */
	MAC4		= 4,
	MAR0		= 8,	/* Multicast filter. */
	CounterAddrLow		= 0x10,
	CounterAddrHigh		= 0x14,
	TxDescStartAddrLow	= 0x20,
	TxDescStartAddrHigh	= 0x24,
	TxHDescStartAddrLow	= 0x28,
	TxHDescStartAddrHigh	= 0x2c,
	FLASH		= 0x30,
	ERSR		= 0x36,
	ChipCmd		= 0x37,
	TxPoll		= 0x38,
	IntrMask	= 0x3c,
	IntrStatus	= 0x3e,
	TxConfig	= 0x40,
	RxConfig	= 0x44,
	RxMissed	= 0x4c,
	Cfg9346		= 0x50,
	Config0		= 0x51,
	Config1		= 0x52,
	Config2		= 0x53,
	Config3		= 0x54,
	Config4		= 0x55,
	Config5		= 0x56,
	MultiIntr	= 0x5c,
	PHYAR		= 0x60,
	PHYstatus	= 0x6c,
	RxMaxSize	= 0xda,
	CPlusCmd	= 0xe0,
	IntrMitigate	= 0xe2,
	RxDescAddrLow	= 0xe4,
	RxDescAddrHigh	= 0xe8,
	EarlyTxThres	= 0xec,
	FuncEvent	= 0xf0,
	FuncEventMask	= 0xf4,
	FuncPresetState	= 0xf8,
	FuncForceEvent	= 0xfc,
};

enum rtl8110_registers {
	TBICSR			= 0x64,
	TBI_ANAR		= 0x68,
	TBI_LPAR		= 0x6a,
};

enum rtl8168_8101_registers {
	CSIDR			= 0x64,
	CSIAR			= 0x68,
#define	CSIAR_FLAG			0x80000000
#define	CSIAR_WRITE_CMD			0x80000000
#define	CSIAR_BYTE_ENABLE		0x0f
#define	CSIAR_BYTE_ENABLE_SHIFT		12
#define	CSIAR_ADDR_MASK			0x0fff

	EPHYAR			= 0x80,
#define	EPHYAR_FLAG			0x80000000
#define	EPHYAR_WRITE_CMD		0x80000000
#define	EPHYAR_REG_MASK			0x1f
#define	EPHYAR_REG_SHIFT		16
#define	EPHYAR_DATA_MASK		0xffff
	DBG_REG			= 0xd1,
#define	FIX_NAK_1			(1 << 4)
#define	FIX_NAK_2			(1 << 3)
};

enum rtl_register_content {
	/* InterruptStatusBits */
	SYSErr		= 0x8000,
	PCSTimeout	= 0x4000,
	SWInt		= 0x0100,
	TxDescUnavail	= 0x0080,
	RxFIFOOver	= 0x0040,
	LinkChg		= 0x0020,
	RxOverflow	= 0x0010,
	TxErr		= 0x0008,
	TxOK		= 0x0004,
	RxErr		= 0x0002,
	RxOK		= 0x0001,

	/* RxStatusDesc */
	RxFOVF	= (1 << 23),
	RxRWT	= (1 << 22),
	RxRES	= (1 << 21),
	RxRUNT	= (1 << 20),
	RxCRC	= (1 << 19),

	/* ChipCmdBits */
	CmdReset	= 0x10,
	CmdRxEnb	= 0x08,
	CmdTxEnb	= 0x04,
	RxBufEmpty	= 0x01,

	/* TXPoll register p.5 */
	HPQ		= 0x80,		/* Poll cmd on the high prio queue */
	NPQ		= 0x40,		/* Poll cmd on the low prio queue */
	FSWInt		= 0x01,		/* Forced software interrupt */

	/* Cfg9346Bits */
	Cfg9346_Lock	= 0x00,
	Cfg9346_Unlock	= 0xc0,

	/* rx_mode_bits */
	AcceptErr	= 0x20,
	AcceptRunt	= 0x10,
	AcceptBroadcast	= 0x08,
	AcceptMulticast	= 0x04,
	AcceptMyPhys	= 0x02,
	AcceptAllPhys	= 0x01,

	/* RxConfigBits */
	RxCfgFIFOShift	= 13,
	RxCfgDMAShift	=  8,

	/* TxConfigBits */
	TxInterFrameGapShift = 24,
	TxDMAShift = 8,	/* DMA burst value (0-7) is shift this many bits */

	/* Config1 register p.24 */
	LEDS1		= (1 << 7),
	LEDS0		= (1 << 6),
	MSIEnable	= (1 << 5),	/* Enable Message Signaled Interrupt */
	Speed_down	= (1 << 4),
	MEMMAP		= (1 << 3),
	IOMAP		= (1 << 2),
	VPD		= (1 << 1),
	PMEnable	= (1 << 0),	/* Power Management Enable */

	/* Config2 register p. 25 */
	PCI_Clock_66MHz = 0x01,
	PCI_Clock_33MHz = 0x00,

	/* Config3 register p.25 */
	MagicPacket	= (1 << 5),	/* Wake up when receives a Magic Packet */
	LinkUp		= (1 << 4),	/* Wake up when the cable connection is re-established */
	Beacon_en	= (1 << 0),	/* 8168 only. Reserved in the 8168b */

	/* Config5 register p.27 */
	BWF		= (1 << 6),	/* Accept Broadcast wakeup frame */
	MWF		= (1 << 5),	/* Accept Multicast wakeup frame */
	UWF		= (1 << 4),	/* Accept Unicast wakeup frame */
	LanWake		= (1 << 1),	/* LanWake enable/disable */
	PMEStatus	= (1 << 0),	/* PME status can be reset by PCI RST# */

	/* TBICSR p.28 */
	TBIReset	= 0x80000000,
	TBILoopback	= 0x40000000,
	TBINwEnable	= 0x20000000,
	TBINwRestart	= 0x10000000,
	TBILinkOk	= 0x02000000,
	TBINwComplete	= 0x01000000,

	/* CPlusCmd p.31 */
	EnableBist	= (1 << 15),	// 8168 8101
	Mac_dbgo_oe	= (1 << 14),	// 8168 8101
	Normal_mode	= (1 << 13),	// unused
	Force_half_dup	= (1 << 12),	// 8168 8101
	Force_rxflow_en	= (1 << 11),	// 8168 8101
	Force_txflow_en	= (1 << 10),	// 8168 8101
	Cxpl_dbg_sel	= (1 << 9),	// 8168 8101
	ASF		= (1 << 8),	// 8168 8101
	PktCntrDisable	= (1 << 7),	// 8168 8101
	Mac_dbgo_sel	= 0x001c,	// 8168
	RxVlan		= (1 << 6),
	RxChkSum	= (1 << 5),
	PCIDAC		= (1 << 4),
	PCIMulRW	= (1 << 3),
	INTT_0		= 0x0000,	// 8168
	INTT_1		= 0x0001,	// 8168
	INTT_2		= 0x0002,	// 8168
	INTT_3		= 0x0003,	// 8168

	/* rtl8169_PHYstatus */
	TBI_Enable	= 0x80,
	TxFlowCtrl	= 0x40,
	RxFlowCtrl	= 0x20,
	_1000bpsF	= 0x10,
	_100bps		= 0x08,
	_10bps		= 0x04,
	LinkStatus	= 0x02,
	FullDup		= 0x01,

	/* _TBICSRBit */
	TBILinkOK	= 0x02000000,

	/* DumpCounterCommand */
	CounterDump	= 0x8,
};

enum desc_status_bit {
	DescOwn		= (1 << 31), /* Descriptor is owned by NIC */
	RingEnd		= (1 << 30), /* End of descriptor ring */
	FirstFrag	= (1 << 29), /* First segment of a packet */
	LastFrag	= (1 << 28), /* Final segment of a packet */

	/* Tx private */
	LargeSend	= (1 << 27), /* TCP Large Send Offload (TSO) */
	MSSShift	= 16,        /* MSS value position */
	MSSMask		= 0xfff,     /* MSS value + LargeSend bit: 12 bits */
	IPCS		= (1 << 18), /* Calculate IP checksum */
	UDPCS		= (1 << 17), /* Calculate UDP/IP checksum */
	TCPCS		= (1 << 16), /* Calculate TCP/IP checksum */
	TxVlanTag	= (1 << 17), /* Add VLAN tag */

	/* Rx private */
	PID1		= (1 << 18), /* Protocol ID bit 1/2 */
	PID0		= (1 << 17), /* Protocol ID bit 2/2 */

#define RxProtoUDP	(PID1)
#define RxProtoTCP	(PID0)
#define RxProtoIP	(PID1 | PID0)
#define RxProtoMask	RxProtoIP

	IPFail		= (1 << 16), /* IP checksum failed */
	UDPFail		= (1 << 15), /* UDP/IP checksum failed */
	TCPFail		= (1 << 14), /* TCP/IP checksum failed */
	RxVlanTag	= (1 << 16), /* VLAN tag available */
};

#define RsvdMask	0x3fffc000

enum features {
	RTL_FEATURE_WOL		= (1 << 0),
	RTL_FEATURE_MSI		= (1 << 1),
	RTL_FEATURE_GMII	= (1 << 2),
};

static void rtl_hw_start_8169 ( struct dev * );
static void rtl_hw_start_8168 ( struct dev * );
static void rtl_hw_start_8101 ( struct dev * );

enum _DescStatusBit {
	OWNbit = 0x80000000,
	EORbit = 0x40000000,
	FSbit  = 0x20000000,
	LSbit  = 0x10000000,
};

struct TxDesc {
	uint32_t status;
	uint32_t vlan_tag;
	uint32_t buf_addr;
	uint32_t buf_Haddr;
};

struct RxDesc {
	uint32_t status;
	uint32_t vlan_tag;
	uint32_t buf_addr;
	uint32_t buf_Haddr;
};



/**
 * The descriptors for this card are required to be aligned on 256
 * byte boundaries.  As the align attribute does not do more than 16
 * bytes of alignment it requires some extra steps.  We add 256 to the
 * size of these arrays and have init_ring adjusts the alignment
 **/

static u8 tx_ring[NUM_TX_DESC * sizeof(struct TxDesc) + 256];
static u8 rx_ring[NUM_RX_DESC * sizeof(struct TxDesc) + 256];

static unsigned char txb[NUM_TX_DESC * TX_BUF_SIZE];
static unsigned char rxb[NUM_RX_DESC * RX_BUF_SIZE];

static uint32_t ioaddr;

struct rtl8169_private {

	struct pci_device *pci_dev;
	uint8_t *hw_addr;
	void *mmio_addr;
	uint32_t irqno;

	int chipset;
	int mac_version;
	int cfg_index;
	u16 intr_event;

	u16 cp_cmd;

	int phy_auto_nego_reg;
	int phy_1000_ctrl_reg;

	int ( *set_speed ) (struct dev *, u8 autoneg, u16 speed, u8 duplex );
	void ( *phy_reset_enable ) ( uint32_t ioaddr );
	void ( *hw_start ) ( struct dev * );
	unsigned int ( *phy_reset_pending ) ( uint32_t ioaddr );
	unsigned int ( *link_ok ) ( uint32_t ioaddr );

	int pcie_cap;

	unsigned features;

	unsigned long cur_rx;
	unsigned long cur_tx;

	struct TxDesc *TxDescArray;	/* Ptr to 256-byte-aligned Tx Descriptor Ring */
	struct RxDesc *RxDescArray;	/* Ptr to 256-byte-aligned Rx Descriptor Ring */

	unsigned char *RxBufferRing[NUM_RX_DESC];	/* Index of Rx Buffer array */
} tpx;

static struct rtl8169_private *tp;

static const u16 rtl8169_intr_mask =
    LinkChg | RxOverflow | RxFIFOOver | TxErr | TxOK | RxErr | RxOK;

static const unsigned int rtl8169_rx_config =
	(RX_FIFO_THRESH << RxCfgFIFOShift) | (RX_DMA_BURST << RxCfgDMAShift);

/*** Low level hardware routines ***/

static void mdio_write(uint32_t ioaddr, int reg_addr, int value)
{
	int i;

	DBGP ( "mdio_write\n" );

	RTL_W32(PHYAR, 0x80000000 | (reg_addr & 0x1f) << 16 | (value & 0xffff));

        for (i = 2000; i > 0; i--) {
		/*
		 * Check if the RTL8169 has completed writing to the specified
		 * MII register.
		 */
		if (!(RTL_R32(PHYAR) & 0x80000000))
			break;
		udelay(100);
	}
}

static int mdio_read(uint32_t ioaddr, int reg_addr)
{
	int i, value = -1;

	DBGP ( "mdio_read\n" );

	RTL_W32(PHYAR, 0x0 | (reg_addr & 0x1f) << 16);

	for (i = 2000; i > 0; i--) {
		/*
		 * Check if the RTL8169 has completed retrieving data from
		 * the specified MII register.
		 */
		if (RTL_R32(PHYAR) & 0x80000000) {
			value = RTL_R32(PHYAR) & 0xffff;
			break;
		}
		udelay(100);
	}
	return value;
}

static void mdio_patch(uint32_t ioaddr, int reg_addr, int value)
{
	DBGP ( "mdio_patch\n" );

	mdio_write(ioaddr, reg_addr, mdio_read(ioaddr, reg_addr) | value);
}

static void rtl_ephy_write(uint32_t ioaddr, int reg_addr, int value)
{
	unsigned int i;

	DBGP ( "rtl_ephy_write\n" );

	RTL_W32(EPHYAR, EPHYAR_WRITE_CMD | (value & EPHYAR_DATA_MASK) |
		(reg_addr & EPHYAR_REG_MASK) << EPHYAR_REG_SHIFT);

	for (i = 0; i < 1000; i++) {
		if (!(RTL_R32(EPHYAR) & EPHYAR_FLAG))
			break;
		udelay(10);
	}
}

static u16 rtl_ephy_read(uint32_t ioaddr, int reg_addr)
{
	u16 value = 0xffff;
	unsigned int i;

	DBGP ( "rtl_ephy_read\n" );

	RTL_W32(EPHYAR, (reg_addr & EPHYAR_REG_MASK) << EPHYAR_REG_SHIFT);

	for (i = 0; i < 1000; i++) {
		if (RTL_R32(EPHYAR) & EPHYAR_FLAG) {
			value = RTL_R32(EPHYAR) & EPHYAR_DATA_MASK;
			break;
		}
		udelay(10);
	}

	return value;
}

static void rtl_csi_write(uint32_t ioaddr, int addr, int value)
{
	unsigned int i;

	DBGP ( "rtl_csi_write\n" );

	RTL_W32(CSIDR, value);
	RTL_W32(CSIAR, CSIAR_WRITE_CMD | (addr & CSIAR_ADDR_MASK) |
		CSIAR_BYTE_ENABLE << CSIAR_BYTE_ENABLE_SHIFT);

	for (i = 0; i < 1000; i++) {
		if (!(RTL_R32(CSIAR) & CSIAR_FLAG))
			break;
		udelay(10);
	}
}

static u32 rtl_csi_read(uint32_t ioaddr, int addr)
{
	u32 value = ~0x00;
	unsigned int i;

	DBGP ( "rtl_csi_read\n" );

	RTL_W32(CSIAR, (addr & CSIAR_ADDR_MASK) |
		CSIAR_BYTE_ENABLE << CSIAR_BYTE_ENABLE_SHIFT);

	for (i = 0; i < 1000; i++) {
		if (RTL_R32(CSIAR) & CSIAR_FLAG) {
			value = RTL_R32(CSIDR);
			break;
		}
		udelay(10);
	}

	return value;
}

static void rtl8169_irq_mask_and_ack(uint32_t ioaddr)
{
	DBGP ( "rtl8169_irq_mask_and_ack\n" );

	RTL_W16(IntrMask, 0x0000);

	RTL_W16(IntrStatus, 0xffff);
}

static unsigned int rtl8169_tbi_reset_pending(uint32_t ioaddr)
{
	DBGP ( "rtl8169_tbi_reset_pending\n" );

	return RTL_R32(TBICSR) & TBIReset;
}

static unsigned int rtl8169_xmii_reset_pending(uint32_t ioaddr)
{
	DBGP ( "rtl8169_xmii_reset_pending\n" );

	return mdio_read(ioaddr, MII_BMCR) & BMCR_RESET;
}

static unsigned int rtl8169_tbi_link_ok(uint32_t ioaddr)
{
	DBGP ( "rtl8169_tbi_link_ok\n" );

	return RTL_R32(TBICSR) & TBILinkOk;
}

static unsigned int rtl8169_xmii_link_ok(uint32_t ioaddr)
{
	DBGP ( "rtl8169_xmii_link_ok\n" );

	return RTL_R8(PHYstatus) & LinkStatus;
}

static void rtl8169_tbi_reset_enable(uint32_t ioaddr)
{
	DBGP ( "rtl8169_tbi_reset_enable\n" );

	RTL_W32(TBICSR, RTL_R32(TBICSR) | TBIReset);
}

static void rtl8169_xmii_reset_enable(uint32_t ioaddr)
{
	unsigned int val;

	DBGP ( "rtl8169_xmii_reset_enable\n" );

	val = mdio_read(ioaddr, MII_BMCR) | BMCR_RESET;
	mdio_write(ioaddr, MII_BMCR, val & 0xffff);
}

static int rtl8169_set_speed_tbi(struct dev *dev __unused,
				 u8 autoneg, u16 speed, u8 duplex)
{
	int ret = 0;
	u32 reg;

	DBGP ( "rtl8169_set_speed_tbi\n" );

	reg = RTL_R32(TBICSR);
	if ((autoneg == AUTONEG_DISABLE) && (speed == SPEED_1000) &&
	    (duplex == DUPLEX_FULL)) {
		RTL_W32(TBICSR, reg & ~(TBINwEnable | TBINwRestart));
	} else if (autoneg == AUTONEG_ENABLE)
		RTL_W32(TBICSR, reg | TBINwEnable | TBINwRestart);
	else {
		DBG ( "incorrect speed setting refused in TBI mode\n" );
		ret = -1;
	}
	return ret;
}

static int rtl8169_set_speed_xmii(struct dev *dev __unused,
				  u8 autoneg, u16 speed, u8 duplex)
{
	int auto_nego, giga_ctrl;

	DBGP ( "rtl8169_set_speed_xmii\n" );

	auto_nego = mdio_read(ioaddr, MII_ADVERTISE);
	auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL |
		       ADVERTISE_100HALF | ADVERTISE_100FULL);
	giga_ctrl = mdio_read(ioaddr, MII_CTRL1000);
	giga_ctrl &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);

	if (autoneg == AUTONEG_ENABLE) {
		auto_nego |= (ADVERTISE_10HALF | ADVERTISE_10FULL |
			      ADVERTISE_100HALF | ADVERTISE_100FULL);
		giga_ctrl |= ADVERTISE_1000FULL | ADVERTISE_1000HALF;
	} else {
		if (speed == SPEED_10)
			auto_nego |= ADVERTISE_10HALF | ADVERTISE_10FULL;
		else if (speed == SPEED_100)
			auto_nego |= ADVERTISE_100HALF | ADVERTISE_100FULL;
		else if (speed == SPEED_1000)
			giga_ctrl |= ADVERTISE_1000FULL | ADVERTISE_1000HALF;

		if (duplex == DUPLEX_HALF)
			auto_nego &= ~(ADVERTISE_10FULL | ADVERTISE_100FULL);

		if (duplex == DUPLEX_FULL)
			auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_100HALF);

		/* This tweak comes straight from Realtek's driver. */
		if ((speed == SPEED_100) && (duplex == DUPLEX_HALF) &&
		    ((tp->mac_version == RTL_GIGA_MAC_VER_13) ||
		     (tp->mac_version == RTL_GIGA_MAC_VER_16))) {
			auto_nego = ADVERTISE_100HALF | ADVERTISE_CSMA;
		}
	}

	/* The 8100e/8101e/8102e do Fast Ethernet only. */
	if ((tp->mac_version == RTL_GIGA_MAC_VER_07) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_08) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_09) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_10) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_13) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_14) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_15) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_16)) {
		if ((giga_ctrl & (ADVERTISE_1000FULL | ADVERTISE_1000HALF))) {
			DBG ( "PHY does not support 1000Mbps.\n" );
		}
		giga_ctrl &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);
	}

	auto_nego |= ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;

	if ((tp->mac_version == RTL_GIGA_MAC_VER_11) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_12) ||
	    (tp->mac_version >= RTL_GIGA_MAC_VER_17)) {
		/*
		 * Wake up the PHY.
		 * Vendor specific (0x1f) and reserved (0x0e) MII registers.
		 */
		mdio_write(ioaddr, 0x1f, 0x0000);
		mdio_write(ioaddr, 0x0e, 0x0000);
	}

	tp->phy_auto_nego_reg = auto_nego;
	tp->phy_1000_ctrl_reg = giga_ctrl;

	mdio_write(ioaddr, MII_ADVERTISE, auto_nego);
	mdio_write(ioaddr, MII_CTRL1000, giga_ctrl);
	mdio_write(ioaddr, MII_BMCR, BMCR_ANENABLE | BMCR_ANRESTART);
	return 0;
}

static int rtl8169_set_speed(struct dev *dev,
			     u8 autoneg, u16 speed, u8 duplex)
{
	int ret;

	DBGP ( "rtl8169_set_speed\n" );

	ret = tp->set_speed(dev, autoneg, speed, duplex);

	return ret;
}

static void rtl8169_write_gmii_reg_bit(uint32_t ioaddr, int reg,
				       int bitnum, int bitval)
{
	int val;

	DBGP ( "rtl8169_write_gmii_reg_bit\n" );

	val = mdio_read(ioaddr, reg);
	val = (bitval == 1) ?
		val | (bitval << bitnum) :  val & ~(0x0001 << bitnum);
	mdio_write(ioaddr, reg, val & 0xffff);
}

static void rtl8169_get_mac_version(struct rtl8169_private *tp,
				    uint32_t ioaddr)
{
	/*
	 * The driver currently handles the 8168Bf and the 8168Be identically
	 * but they can be identified more specifically through the test below
	 * if needed:
	 *
	 * (RTL_R32(TxConfig) & 0x700000) == 0x500000 ? 8168Bf : 8168Be
	 *
	 * Same thing for the 8101Eb and the 8101Ec:
	 *
	 * (RTL_R32(TxConfig) & 0x700000) == 0x200000 ? 8101Eb : 8101Ec
	 */
	const struct {
		u32 mask;
		u32 val;
		int mac_version;
	} mac_info[] = {
		/* 8168D family. */
		{ 0x7c800000, 0x28000000,	RTL_GIGA_MAC_VER_25 },

		/* 8168C family. */
		{ 0x7cf00000, 0x3ca00000,	RTL_GIGA_MAC_VER_24 },
		{ 0x7cf00000, 0x3c900000,	RTL_GIGA_MAC_VER_23 },
		{ 0x7cf00000, 0x3c800000,	RTL_GIGA_MAC_VER_18 },
		{ 0x7c800000, 0x3c800000,	RTL_GIGA_MAC_VER_24 },
		{ 0x7cf00000, 0x3c000000,	RTL_GIGA_MAC_VER_19 },
		{ 0x7cf00000, 0x3c200000,	RTL_GIGA_MAC_VER_20 },
		{ 0x7cf00000, 0x3c300000,	RTL_GIGA_MAC_VER_21 },
		{ 0x7cf00000, 0x3c400000,	RTL_GIGA_MAC_VER_22 },
		{ 0x7c800000, 0x3c000000,	RTL_GIGA_MAC_VER_22 },

		/* 8168B family. */
		{ 0x7cf00000, 0x38000000,	RTL_GIGA_MAC_VER_12 },
		{ 0x7cf00000, 0x38500000,	RTL_GIGA_MAC_VER_17 },
		{ 0x7c800000, 0x38000000,	RTL_GIGA_MAC_VER_17 },
		{ 0x7c800000, 0x30000000,	RTL_GIGA_MAC_VER_11 },

		/* 8101 family. */
		{ 0x7cf00000, 0x34a00000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7cf00000, 0x24a00000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7cf00000, 0x34900000,	RTL_GIGA_MAC_VER_08 },
		{ 0x7cf00000, 0x24900000,	RTL_GIGA_MAC_VER_08 },
		{ 0x7cf00000, 0x34800000,	RTL_GIGA_MAC_VER_07 },
		{ 0x7cf00000, 0x24800000,	RTL_GIGA_MAC_VER_07 },
		{ 0x7cf00000, 0x34000000,	RTL_GIGA_MAC_VER_13 },
		{ 0x7cf00000, 0x34300000,	RTL_GIGA_MAC_VER_10 },
		{ 0x7cf00000, 0x34200000,	RTL_GIGA_MAC_VER_16 },
		{ 0x7c800000, 0x34800000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7c800000, 0x24800000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7c800000, 0x34000000,	RTL_GIGA_MAC_VER_16 },
		{ 0xfc800000, 0x38800000,	RTL_GIGA_MAC_VER_15 },
		{ 0xfc800000, 0x30800000,	RTL_GIGA_MAC_VER_14 },

		/* 8110 family. */
		{ 0xfc800000, 0x98000000,	RTL_GIGA_MAC_VER_06 },
		{ 0xfc800000, 0x18000000,	RTL_GIGA_MAC_VER_05 },
		{ 0xfc800000, 0x10000000,	RTL_GIGA_MAC_VER_04 },
		{ 0xfc800000, 0x04000000,	RTL_GIGA_MAC_VER_03 },
		{ 0xfc800000, 0x00800000,	RTL_GIGA_MAC_VER_02 },
		{ 0xfc800000, 0x00000000,	RTL_GIGA_MAC_VER_01 },

		{ 0x00000000, 0x00000000,	RTL_GIGA_MAC_VER_01 }	/* Catch-all */
	}, *p = mac_info;
	u32 reg;

	DBGP ( "rtl8169_get_mac_version\n" );

	reg = RTL_R32(TxConfig);
	while ((reg & p->mask) != p->val)
		p++;
	tp->mac_version = p->mac_version;

	DBG ( "tp->mac_version = %d\n", tp->mac_version );

	if (p->mask == 0x00000000) {
		DBG ( "unknown MAC (%08x)\n", reg );
	}
}

static void
rtl_get_chipset ()
{
        int i;

	DBGP ( "rtl_get_chipset\n" );

	for ( i = 0; (u32) i < ARRAY_SIZE ( rtl_chip_info ); i++ ) {
		if ( tp->mac_version == rtl_chip_info[i].mac_version )
			break;
	}
	if ( i == ARRAY_SIZE(rtl_chip_info ) ) {
		/* Unknown chip: assume array element #0, original RTL-8169 */
		DBG ( "Unknown chip version, assuming %s\n", rtl_chip_info[0].name );
		i = 0;
	}
	tp->chipset = i;

	if ((tp->mac_version <= RTL_GIGA_MAC_VER_06) &&
	    (RTL_R8(PHYstatus) & TBI_Enable)) {
		tp->set_speed = rtl8169_set_speed_tbi;
		tp->phy_reset_enable = rtl8169_tbi_reset_enable;
		tp->phy_reset_pending = rtl8169_tbi_reset_pending;
		tp->link_ok = rtl8169_tbi_link_ok;
		tp->phy_1000_ctrl_reg = ADVERTISE_1000FULL; /* Implied by TBI */
	} else {
		tp->set_speed = rtl8169_set_speed_xmii;
		tp->phy_reset_enable = rtl8169_xmii_reset_enable;
		tp->phy_reset_pending = rtl8169_xmii_reset_pending;
		tp->link_ok = rtl8169_xmii_link_ok;
	}

}

struct phy_reg {
	u16 reg;
	u16 val;
};

static void rtl_phy_write(uint32_t ioaddr, struct phy_reg *regs, int len)
{
	DBGP ( "rtl_phy_write\n" );

	while (len-- > 0) {
		mdio_write(ioaddr, regs->reg, regs->val);
		regs++;
	}
}

static void rtl8169s_hw_phy_config(uint32_t ioaddr)
{
	struct {
		u16 regs[5]; /* Beware of bit-sign propagation */
	} phy_magic[5] = { {
		{ 0x0000,	//w 4 15 12 0
		  0x00a1,	//w 3 15 0 00a1
		  0x0008,	//w 2 15 0 0008
		  0x1020,	//w 1 15 0 1020
		  0x1000 } },{	//w 0 15 0 1000
		{ 0x7000,	//w 4 15 12 7
		  0xff41,	//w 3 15 0 ff41
		  0xde60,	//w 2 15 0 de60
		  0x0140,	//w 1 15 0 0140
		  0x0077 } },{	//w 0 15 0 0077
		{ 0xa000,	//w 4 15 12 a
		  0xdf01,	//w 3 15 0 df01
		  0xdf20,	//w 2 15 0 df20
		  0xff95,	//w 1 15 0 ff95
		  0xfa00 } },{	//w 0 15 0 fa00
		{ 0xb000,	//w 4 15 12 b
		  0xff41,	//w 3 15 0 ff41
		  0xde20,	//w 2 15 0 de20
		  0x0140,	//w 1 15 0 0140
		  0x00bb } },{	//w 0 15 0 00bb
		{ 0xf000,	//w 4 15 12 f
		  0xdf01,	//w 3 15 0 df01
		  0xdf20,	//w 2 15 0 df20
		  0xff95,	//w 1 15 0 ff95
		  0xbf00 }	//w 0 15 0 bf00
		}
	}, *p = phy_magic;
	unsigned int i;

	DBGP ( "rtl8169s_hw_phy_config\n" );

	mdio_write(ioaddr, 0x1f, 0x0001);		//w 31 2 0 1
	mdio_write(ioaddr, 0x15, 0x1000);		//w 21 15 0 1000
	mdio_write(ioaddr, 0x18, 0x65c7);		//w 24 15 0 65c7
	rtl8169_write_gmii_reg_bit(ioaddr, 4, 11, 0);	//w 4 11 11 0

	for (i = 0; i < ARRAY_SIZE(phy_magic); i++, p++) {
		int val, pos = 4;

		val = (mdio_read(ioaddr, pos) & 0x0fff) | (p->regs[0] & 0xffff);
		mdio_write(ioaddr, pos, val);
		while (--pos >= 0)
			mdio_write(ioaddr, pos, p->regs[4 - pos] & 0xffff);
		rtl8169_write_gmii_reg_bit(ioaddr, 4, 11, 1); //w 4 11 11 1
		rtl8169_write_gmii_reg_bit(ioaddr, 4, 11, 0); //w 4 11 11 0
	}
	mdio_write(ioaddr, 0x1f, 0x0000); //w 31 2 0 0
}

static void rtl8169sb_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0002 },
		{ 0x01, 0x90d0 },
		{ 0x1f, 0x0000 }
	};

	DBGP ( "rtl8169sb_hw_phy_config\n" );

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168bb_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x10, 0xf41b },
		{ 0x1f, 0x0000 }
	};

	mdio_write(ioaddr, 0x1f, 0x0001);
	mdio_patch(ioaddr, 0x16, 1 << 0);

	DBGP ( "rtl8168bb_hw_phy_config\n" );

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168bef_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x10, 0xf41b },
		{ 0x1f, 0x0000 }
	};

	DBGP ( "rtl8168bef_hw_phy_config\n" );

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168cp_1_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0000 },
		{ 0x1d, 0x0f00 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x1ec8 },
		{ 0x1f, 0x0000 }
	};

	DBGP ( "rtl8168cp_1_hw_phy_config\n" );

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168cp_2_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x1d, 0x3d98 },
		{ 0x1f, 0x0000 }
	};

	DBGP ( "rtl8168cp_2_hw_phy_config\n" );

	mdio_write(ioaddr, 0x1f, 0x0000);
	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168c_1_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x1f, 0x0002 },
		{ 0x00, 0x88d4 },
		{ 0x01, 0x82b1 },
		{ 0x03, 0x7002 },
		{ 0x08, 0x9e30 },
		{ 0x09, 0x01f0 },
		{ 0x0a, 0x5500 },
		{ 0x0c, 0x00c8 },
		{ 0x1f, 0x0003 },
		{ 0x12, 0xc096 },
		{ 0x16, 0x000a },
		{ 0x1f, 0x0000 },
		{ 0x1f, 0x0000 },
		{ 0x09, 0x2000 },
		{ 0x09, 0x0000 }
	};

	DBGP ( "rtl8168c_1_hw_phy_config\n" );

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8168c_2_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x03, 0x802f },
		{ 0x02, 0x4f02 },
		{ 0x01, 0x0409 },
		{ 0x00, 0xf099 },
		{ 0x04, 0x9800 },
		{ 0x04, 0x9000 },
		{ 0x1d, 0x3d98 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x7eb8 },
		{ 0x06, 0x0761 },
		{ 0x1f, 0x0003 },
		{ 0x16, 0x0f0a },
		{ 0x1f, 0x0000 }
	};

	DBGP ( "rtl8168c_2_hw_phy_config\n" );

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	mdio_patch(ioaddr, 0x16, 1 << 0);
	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8168c_3_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x1d, 0x3d98 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x7eb8 },
		{ 0x06, 0x5461 },
		{ 0x1f, 0x0003 },
		{ 0x16, 0x0f0a },
		{ 0x1f, 0x0000 }
	};

	DBGP ( "rtl8168c_3_hw_phy_config\n" );

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	mdio_patch(ioaddr, 0x16, 1 << 0);
	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8168c_4_hw_phy_config(uint32_t ioaddr)
{
	DBGP ( "rtl8168c_4_hw_phy_config\n" );

	rtl8168c_3_hw_phy_config(ioaddr);
}

static void rtl8168d_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init_0[] = {
		{ 0x1f, 0x0001 },
		{ 0x09, 0x2770 },
		{ 0x08, 0x04d0 },
		{ 0x0b, 0xad15 },
		{ 0x0c, 0x5bf0 },
		{ 0x1c, 0xf101 },
		{ 0x1f, 0x0003 },
		{ 0x14, 0x94d7 },
		{ 0x12, 0xf4d6 },
		{ 0x09, 0xca0f },
		{ 0x1f, 0x0002 },
		{ 0x0b, 0x0b10 },
		{ 0x0c, 0xd1f7 },
		{ 0x1f, 0x0002 },
		{ 0x06, 0x5461 },
		{ 0x1f, 0x0002 },
		{ 0x05, 0x6662 },
		{ 0x1f, 0x0000 },
		{ 0x14, 0x0060 },
		{ 0x1f, 0x0000 },
		{ 0x0d, 0xf8a0 },
		{ 0x1f, 0x0005 },
		{ 0x05, 0xffc2 }
	};

	DBGP ( "rtl8168d_hw_phy_config\n" );

	rtl_phy_write(ioaddr, phy_reg_init_0, ARRAY_SIZE(phy_reg_init_0));

	if (mdio_read(ioaddr, 0x06) == 0xc400) {
		struct phy_reg phy_reg_init_1[] = {
			{ 0x1f, 0x0005 },
			{ 0x01, 0x0300 },
			{ 0x1f, 0x0000 },
			{ 0x11, 0x401c },
			{ 0x16, 0x4100 },
			{ 0x1f, 0x0005 },
			{ 0x07, 0x0010 },
			{ 0x05, 0x83dc },
			{ 0x06, 0x087d },
			{ 0x05, 0x8300 },
			{ 0x06, 0x0101 },
			{ 0x06, 0x05f8 },
			{ 0x06, 0xf9fa },
			{ 0x06, 0xfbef },
			{ 0x06, 0x79e2 },
			{ 0x06, 0x835f },
			{ 0x06, 0xe0f8 },
			{ 0x06, 0x9ae1 },
			{ 0x06, 0xf89b },
			{ 0x06, 0xef31 },
			{ 0x06, 0x3b65 },
			{ 0x06, 0xaa07 },
			{ 0x06, 0x81e4 },
			{ 0x06, 0xf89a },
			{ 0x06, 0xe5f8 },
			{ 0x06, 0x9baf },
			{ 0x06, 0x06ae },
			{ 0x05, 0x83dc },
			{ 0x06, 0x8300 },
		};

		rtl_phy_write(ioaddr, phy_reg_init_1,
			      ARRAY_SIZE(phy_reg_init_1));
	}

	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8102e_hw_phy_config(uint32_t ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0003 },
		{ 0x08, 0x441d },
		{ 0x01, 0x9100 },
		{ 0x1f, 0x0000 }
	};

	DBGP ( "rtl8102e_hw_phy_config\n" );

	mdio_write(ioaddr, 0x1f, 0x0000);
	mdio_patch(ioaddr, 0x11, 1 << 12);
	mdio_patch(ioaddr, 0x19, 1 << 13);

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl_hw_phy_config(struct dev *dev __unused)
{
	DBGP ( "rtl_hw_phy_config\n" );

	DBG ( "mac_version = 0x%02x\n", tp->mac_version );

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_01:
		break;
	case RTL_GIGA_MAC_VER_02:
	case RTL_GIGA_MAC_VER_03:
		rtl8169s_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_04:
		rtl8169sb_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_07:
	case RTL_GIGA_MAC_VER_08:
	case RTL_GIGA_MAC_VER_09:
		rtl8102e_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_11:
		rtl8168bb_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_12:
		rtl8168bef_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_17:
		rtl8168bef_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_18:
		rtl8168cp_1_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_19:
		rtl8168c_1_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_20:
		rtl8168c_2_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_21:
		rtl8168c_3_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_22:
		rtl8168c_4_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_23:
	case RTL_GIGA_MAC_VER_24:
		rtl8168cp_2_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_25:
		rtl8168d_hw_phy_config(ioaddr);
		break;

	default:
		break;
	}
}

static void rtl8169_phy_reset(struct dev *dev __unused,
			      struct rtl8169_private *tp)
{
	unsigned int i;

	DBGP ( "rtl8169_phy_reset\n" );

	tp->phy_reset_enable(ioaddr);
	for (i = 0; i < 100; i++) {
		if (!tp->phy_reset_pending(ioaddr))
			return;
		mdelay ( 1 );
	}
	printf ( "PHY reset failed.\n" );
}

static void rtl8169_init_phy(struct dev *dev, struct rtl8169_private *tp)
{
	DBGP ( "rtl8169_init_phy\n" );

	rtl_hw_phy_config ( dev );

	if (tp->mac_version <= RTL_GIGA_MAC_VER_06) {
		DBG ( "Set MAC Reg C+CR Offset 0x82h = 0x01h\n" );
		RTL_W8(0x82, 0x01);
	}

	pci_write_config_byte(tp->pci_dev, PCI_LATENCY_TIMER, 0x40);

	if (tp->mac_version <= RTL_GIGA_MAC_VER_06)
		pci_write_config_byte(tp->pci_dev, PCI_CACHE_LINE_SIZE, 0x08);

	if (tp->mac_version == RTL_GIGA_MAC_VER_02) {
		DBG ( "Set MAC Reg C+CR Offset 0x82h = 0x01h\n" );
		RTL_W8(0x82, 0x01);
		DBG ( "Set PHY Reg 0x0bh = 0x00h\n" );
		mdio_write(ioaddr, 0x0b, 0x0000); //w 0x0b 15 0 0
	}

	rtl8169_phy_reset(dev, tp);

	/*
	 * rtl8169_set_speed_xmii takes good care of the Fast Ethernet
	 * only 8101. Don't panic.
	 */
	rtl8169_set_speed(dev, AUTONEG_ENABLE, SPEED_1000, DUPLEX_FULL);

	if ((RTL_R8(PHYstatus) & TBI_Enable)) {
		DBG ( "TBI auto-negotiating\n" );
        }
}

static const struct rtl_cfg_info {
	void (*hw_start)(struct dev *);
	unsigned int region;
	unsigned int align;
	u16 intr_event;
	u16 napi_event;
	unsigned features;
} rtl_cfg_infos [] = {
	[RTL_CFG_0] = {
		.hw_start	= rtl_hw_start_8169,
		.region		= 1,
		.align		= 0,
		.intr_event	= SYSErr | LinkChg | RxOverflow |
				  RxFIFOOver | TxErr | TxOK | RxOK | RxErr,
		.napi_event	= RxFIFOOver | TxErr | TxOK | RxOK | RxOverflow,
		.features	= RTL_FEATURE_GMII
	},
	[RTL_CFG_1] = {
		.hw_start	= rtl_hw_start_8168,
		.region		= 2,
		.align		= 8,
		.intr_event	= SYSErr | LinkChg | RxOverflow |
				  TxErr | TxOK | RxOK | RxErr,
		.napi_event	= TxErr | TxOK | RxOK | RxOverflow,
		.features	= RTL_FEATURE_GMII
	},
	[RTL_CFG_2] = {
		.hw_start	= rtl_hw_start_8101,
		.region		= 2,
		.align		= 8,
		.intr_event	= SYSErr | LinkChg | RxOverflow | PCSTimeout |
				  RxFIFOOver | TxErr | TxOK | RxOK | RxErr,
		.napi_event	= RxFIFOOver | TxErr | TxOK | RxOK | RxOverflow,
	}
};

static void rtl8169_hw_reset(uint32_t ioaddr)
{
        int i;

	DBGP ( "rtl8169_hw_reset\n" );

	/* Disable interrupts */
	rtl8169_irq_mask_and_ack(ioaddr);

	/* Reset the chipset */
	RTL_W8(ChipCmd, CmdReset);

        /* Check that the chip has finished the reset. */
	for ( i = 0; i < 1000; i++ ) {
		if ( ( RTL_R8 ( ChipCmd ) & CmdReset ) == 0 )
			break;
		mdelay ( 1 );
	}

	if ( i == 1000 ) {
		printf ( "Reset Failed! ( > 1000 iterations )\n" );
	}
}

static void rtl_set_rx_tx_config_registers(struct rtl8169_private *tp)
{
	u32 cfg = rtl8169_rx_config;

	DBGP ( "rtl_set_rx_tx_config_registers\n" );

	cfg |= (RTL_R32(RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);
	RTL_W32(RxConfig, cfg);

	/* Set DMA burst size and Interframe Gap Time */
	RTL_W32(TxConfig, (TX_DMA_BURST << TxDMAShift) |
		(InterFrameGap << TxInterFrameGapShift));
}

static void rtl_soft_reset ( struct dev *dev __unused )
{
	unsigned int i;

	DBGP ( "rtl_soft_reset\n" );

	/* Soft reset the chip. */
	RTL_W8 ( ChipCmd, CmdReset );

        /* Check that the chip has finished the reset. */
	for ( i = 0; i < 1000; i++ ) {
		if ( ( RTL_R8 ( ChipCmd ) & CmdReset ) == 0 )
			break;
		mdelay ( 1 );
	}

	if ( i == 1000 ) {
		printf ( "Reset Failed! ( > 1000 iterations )\n" );
	}
}

static void rtl_hw_start ( struct dev *dev )
{
	DBGP ( "rtl_hw_start\n" );

	/* Soft reset NIC */
	rtl_soft_reset ( dev );

	tp->hw_start ( dev );
}

static void rtl_set_rx_tx_desc_registers ( struct rtl8169_private *tp,
					 uint32_t ioaddr )
{
	DBGP ( "rtl_set_rx_tx_desc_registers\n" );

	/*
	 * Magic spell: some iop3xx ARM board needs the TxDescAddrHigh
	 * register to be written before TxDescAddrLow to work.
	 * Switching from MMIO to I/O access fixes the issue as well.
	 */
	RTL_W32 ( TxDescStartAddrHigh, 0 );
	RTL_W32 ( TxDescStartAddrLow, virt_to_bus ( tp->TxDescArray ) );
	RTL_W32 ( RxDescAddrHigh, 0 );
	RTL_W32 ( RxDescAddrLow, virt_to_bus ( tp->RxDescArray ) );
}

static u16 rtl_rw_cpluscmd(uint32_t ioaddr)
{
	u16 cmd;

	DBGP ( "rtl_rw_cpluscmd\n" );

	cmd = RTL_R16(CPlusCmd);
	RTL_W16(CPlusCmd, cmd);
	return cmd;
}

static void rtl_set_rx_max_size(uint32_t ioaddr)
{
	DBGP ( "rtl_set_rx_max_size\n" );

	RTL_W16 ( RxMaxSize, RX_BUF_SIZE );
}

static void rtl8169_set_magic_reg(uint32_t ioaddr, unsigned mac_version)
{
	struct {
		u32 mac_version;
		u32 clk;
		u32 val;
	} cfg2_info [] = {
		{ RTL_GIGA_MAC_VER_05, PCI_Clock_33MHz, 0x000fff00 }, // 8110SCd
		{ RTL_GIGA_MAC_VER_05, PCI_Clock_66MHz, 0x000fffff },
		{ RTL_GIGA_MAC_VER_06, PCI_Clock_33MHz, 0x00ffff00 }, // 8110SCe
		{ RTL_GIGA_MAC_VER_06, PCI_Clock_66MHz, 0x00ffffff }
	}, *p = cfg2_info;
	unsigned int i;
	u32 clk;

	DBGP ( "rtl8169_set_magic_reg\n" );

	clk = RTL_R8(Config2) & PCI_Clock_66MHz;
	for (i = 0; i < ARRAY_SIZE(cfg2_info); i++, p++) {
		if ((p->mac_version == mac_version) && (p->clk == clk)) {
			RTL_W32(0x7c, p->val);
			break;
		}
	}
}

static void rtl_set_rx_mode ( struct dev *dev __unused )
{
	u32 tmp;

	DBGP ( "rtl_set_rx_mode\n" );

	/* Accept all Multicast Packets */

	RTL_W32 ( MAR0 + 0, 0xffffffff );
	RTL_W32 ( MAR0 + 4, 0xffffffff );

	tmp = rtl8169_rx_config | AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
	      ( RTL_R32 ( RxConfig ) & rtl_chip_info[tp->chipset].RxConfigMask );

	RTL_W32 ( RxConfig, tmp );
}

static void rtl_hw_start_8169(struct dev *dev)
{
	struct pci_device *pdev = tp->pci_dev;

	DBGP ( "rtl_hw_start_8169\n" );

	if (tp->mac_version == RTL_GIGA_MAC_VER_05) {
		RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) | PCIMulRW);
		pci_write_config_byte(pdev, PCI_CACHE_LINE_SIZE, 0x08);
	}

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	if ((tp->mac_version == RTL_GIGA_MAC_VER_01) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_04))
		RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_set_rx_max_size(ioaddr);

	if ((tp->mac_version == RTL_GIGA_MAC_VER_01) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_04))
		rtl_set_rx_tx_config_registers(tp);

	tp->cp_cmd |= rtl_rw_cpluscmd(ioaddr) | PCIMulRW;

	if ((tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03)) {
		DBG ( "Set MAC Reg C+CR Offset 0xE0. "
			"Bit-3 and bit-14 MUST be 1\n" );
		tp->cp_cmd |= (1 << 14);
	}

	RTL_W16(CPlusCmd, tp->cp_cmd);

	rtl8169_set_magic_reg(ioaddr, tp->mac_version);

	/*
	 * Undocumented corner. Supposedly:
	 * (TxTimer << 12) | (TxPackets << 8) | (RxTimer << 4) | RxPackets
	 */
	RTL_W16(IntrMitigate, 0x0000);

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	if ((tp->mac_version != RTL_GIGA_MAC_VER_01) &&
	    (tp->mac_version != RTL_GIGA_MAC_VER_02) &&
	    (tp->mac_version != RTL_GIGA_MAC_VER_03) &&
	    (tp->mac_version != RTL_GIGA_MAC_VER_04)) {
		RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);
		rtl_set_rx_tx_config_registers(tp);
	}

	RTL_W8(Cfg9346, Cfg9346_Lock);

	/* Initially a 10 us delay. Turned it into a PCI commit. - FR */
	RTL_R8(IntrMask);

	RTL_W32(RxMissed, 0);

	rtl_set_rx_mode(dev);

	/* no early-rx interrupts */
	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xF000);

	//        RTL_W16(IntrMask, tp->intr_event);
}

static void rtl_tx_performance_tweak(struct pci_device *pdev, u16 force)
{
	int cap = tp->pcie_cap;

	DBGP ( "rtl_tx_performance_tweak\n" );

	if (cap) {
		u16 ctl;

		pci_read_config_word(pdev, cap + PCI_EXP_DEVCTL, &ctl);
		ctl = (ctl & ~PCI_EXP_DEVCTL_READRQ) | force;
		pci_write_config_word(pdev, cap + PCI_EXP_DEVCTL, ctl);
	}
}

static void rtl_csi_access_enable(uint32_t ioaddr)
{
	u32 csi;

	DBGP ( "rtl_csi_access_enable\n" );

	csi = rtl_csi_read(ioaddr, 0x070c) & 0x00ffffff;
	rtl_csi_write(ioaddr, 0x070c, csi | 0x27000000);
}

struct ephy_info {
	unsigned int offset;
	u16 mask;
	u16 bits;
};

static void rtl_ephy_init(uint32_t ioaddr, struct ephy_info *e, int len)
{
	u16 w;

	DBGP ( "rtl_ephy_init\n" );

	while (len-- > 0) {
		w = (rtl_ephy_read(ioaddr, e->offset) & ~e->mask) | e->bits;
		rtl_ephy_write(ioaddr, e->offset, w);
		e++;
	}
}

static void rtl_disable_clock_request(struct pci_device *pdev)
{
	int cap = tp->pcie_cap;

	DBGP ( "rtl_disable_clock_request\n" );

	if (cap) {
		u16 ctl;

		pci_read_config_word(pdev, cap + PCI_EXP_LNKCTL, &ctl);
		ctl &= ~PCI_EXP_LNKCTL_CLKREQ_EN;
		pci_write_config_word(pdev, cap + PCI_EXP_LNKCTL, ctl);
	}
}

#define R8168_CPCMD_QUIRK_MASK (\
	EnableBist | \
	Mac_dbgo_oe | \
	Force_half_dup | \
	Force_rxflow_en | \
	Force_txflow_en | \
	Cxpl_dbg_sel | \
	ASF | \
	PktCntrDisable | \
	Mac_dbgo_sel)

static void rtl_hw_start_8168bb(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "rtl_hw_start_8168bb\n" );

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);

	rtl_tx_performance_tweak(pdev,
		(0x5 << MAX_READ_REQUEST_SHIFT) | PCI_EXP_DEVCTL_NOSNOOP_EN);
}

static void rtl_hw_start_8168bef(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "rtl_hw_start_8168bef\n" );

	rtl_hw_start_8168bb(ioaddr, pdev);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	RTL_W8(Config4, RTL_R8(Config4) & ~(1 << 0));
}

static void __rtl_hw_start_8168cp(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "__rtl_hw_start_8168cp\n" );

	RTL_W8(Config1, RTL_R8(Config1) | Speed_down);

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	rtl_disable_clock_request(pdev);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168cp_1(uint32_t ioaddr, struct pci_device *pdev)
{
	static struct ephy_info e_info_8168cp[] = {
		{ 0x01, 0,	0x0001 },
		{ 0x02, 0x0800,	0x1000 },
		{ 0x03, 0,	0x0042 },
		{ 0x06, 0x0080,	0x0000 },
		{ 0x07, 0,	0x2000 }
	};

	DBGP ( "rtl_hw_start_8168cp_1\n" );

	rtl_csi_access_enable(ioaddr);

	rtl_ephy_init(ioaddr, e_info_8168cp, ARRAY_SIZE(e_info_8168cp));

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168cp_2(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "rtl_hw_start_8168cp_2\n" );

	rtl_csi_access_enable(ioaddr);

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168cp_3(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "rtl_hw_start_8168cp_3\n" );

	rtl_csi_access_enable(ioaddr);

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	/* Magic. */
	RTL_W8(DBG_REG, 0x20);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168c_1(uint32_t ioaddr, struct pci_device *pdev)
{
	static struct ephy_info e_info_8168c_1[] = {
		{ 0x02, 0x0800,	0x1000 },
		{ 0x03, 0,	0x0002 },
		{ 0x06, 0x0080,	0x0000 }
	};

	DBGP ( "rtl_hw_start_8168c_1\n" );

	rtl_csi_access_enable(ioaddr);

	RTL_W8(DBG_REG, 0x06 | FIX_NAK_1 | FIX_NAK_2);

	rtl_ephy_init(ioaddr, e_info_8168c_1, ARRAY_SIZE(e_info_8168c_1));

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168c_2(uint32_t ioaddr, struct pci_device *pdev)
{
	static struct ephy_info e_info_8168c_2[] = {
		{ 0x01, 0,	0x0001 },
		{ 0x03, 0x0400,	0x0220 }
	};

	DBGP ( "rtl_hw_start_8168c_2\n" );

	rtl_csi_access_enable(ioaddr);

	rtl_ephy_init(ioaddr, e_info_8168c_2, ARRAY_SIZE(e_info_8168c_2));

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168c_3(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "rtl_hw_start_8168c_3\n" );

	rtl_hw_start_8168c_2(ioaddr, pdev);
}

static void rtl_hw_start_8168c_4(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "rtl_hw_start_8168c_4\n" );

	rtl_csi_access_enable(ioaddr);

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168d(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "rtl_hw_start_8168d\n" );

	rtl_csi_access_enable(ioaddr);

	rtl_disable_clock_request(pdev);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168(struct dev *dev)
{
	struct pci_device *pdev = tp->pci_dev;

	DBGP ( "rtl_hw_start_8168\n" );

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_set_rx_max_size(ioaddr);

	tp->cp_cmd |= RTL_R16(CPlusCmd) | PktCntrDisable | INTT_1;

	RTL_W16(CPlusCmd, tp->cp_cmd);

	RTL_W16(IntrMitigate, 0x5151);

	/* Work around for RxFIFO overflow. */
	if (tp->mac_version == RTL_GIGA_MAC_VER_11) {
		tp->intr_event |= RxFIFOOver | PCSTimeout;
		tp->intr_event &= ~RxOverflow;
	}

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	rtl_set_rx_mode(dev);

	RTL_W32(TxConfig, (TX_DMA_BURST << TxDMAShift) |
		(InterFrameGap << TxInterFrameGapShift));

	RTL_R8(IntrMask);

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_11:
		rtl_hw_start_8168bb(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_12:
	case RTL_GIGA_MAC_VER_17:
		rtl_hw_start_8168bef(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_18:
		rtl_hw_start_8168cp_1(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_19:
		rtl_hw_start_8168c_1(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_20:
		rtl_hw_start_8168c_2(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_21:
		rtl_hw_start_8168c_3(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_22:
		rtl_hw_start_8168c_4(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_23:
		rtl_hw_start_8168cp_2(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_24:
		rtl_hw_start_8168cp_3(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_25:
		rtl_hw_start_8168d(ioaddr, pdev);
	break;

	default:
		DBG ( "Unknown chipset (mac_version = %d).\n",
		      tp->mac_version );
	break;
	}

	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W8(Cfg9346, Cfg9346_Lock);

	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xF000);

	//        RTL_W16(IntrMask, tp->intr_event);
}

#define R810X_CPCMD_QUIRK_MASK (\
	EnableBist | \
	Mac_dbgo_oe | \
	Force_half_dup | \
	Force_half_dup | \
	Force_txflow_en | \
	Cxpl_dbg_sel | \
	ASF | \
	PktCntrDisable | \
	PCIDAC | \
	PCIMulRW)

static void rtl_hw_start_8102e_1(uint32_t ioaddr, struct pci_device *pdev)
{
	static struct ephy_info e_info_8102e_1[] = {
		{ 0x01,	0, 0x6e65 },
		{ 0x02,	0, 0x091f },
		{ 0x03,	0, 0xc2f9 },
		{ 0x06,	0, 0xafb5 },
		{ 0x07,	0, 0x0e00 },
		{ 0x19,	0, 0xec80 },
		{ 0x01,	0, 0x2e65 },
		{ 0x01,	0, 0x6e65 }
	};
	u8 cfg1;

	DBGP ( "rtl_hw_start_8102e_1\n" );

	rtl_csi_access_enable(ioaddr);

	RTL_W8(DBG_REG, FIX_NAK_1);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W8(Config1,
	       LEDS1 | LEDS0 | Speed_down | MEMMAP | IOMAP | VPD | PMEnable);
	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	cfg1 = RTL_R8(Config1);
	if ((cfg1 & LEDS0) && (cfg1 & LEDS1))
		RTL_W8(Config1, cfg1 & ~LEDS0);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R810X_CPCMD_QUIRK_MASK);

	rtl_ephy_init(ioaddr, e_info_8102e_1, ARRAY_SIZE(e_info_8102e_1));
}

static void rtl_hw_start_8102e_2(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "rtl_hw_start_8102e_2\n" );

	rtl_csi_access_enable(ioaddr);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W8(Config1, MEMMAP | IOMAP | VPD | PMEnable);
	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R810X_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8102e_3(uint32_t ioaddr, struct pci_device *pdev)
{
	DBGP ( "rtl_hw_start_8102e_3\n" );

	rtl_hw_start_8102e_2(ioaddr, pdev);

	rtl_ephy_write(ioaddr, 0x03, 0xc2f9);
}

static void rtl_hw_start_8101(struct dev *dev)
{
	struct pci_device *pdev = tp->pci_dev;

	DBGP ( "rtl_hw_start_8101\n" );

	if ((tp->mac_version == RTL_GIGA_MAC_VER_13) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_16)) {
		int cap = tp->pcie_cap;

		if (cap) {
			pci_write_config_word(pdev, cap + PCI_EXP_DEVCTL,
					      PCI_EXP_DEVCTL_NOSNOOP_EN);
		}
	}

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_07:
		rtl_hw_start_8102e_1(ioaddr, pdev);
		break;

	case RTL_GIGA_MAC_VER_08:
		rtl_hw_start_8102e_3(ioaddr, pdev);
		break;

	case RTL_GIGA_MAC_VER_09:
		rtl_hw_start_8102e_2(ioaddr, pdev);
		break;
	}

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_set_rx_max_size(ioaddr);

	tp->cp_cmd |= rtl_rw_cpluscmd(ioaddr) | PCIMulRW;

	RTL_W16(CPlusCmd, tp->cp_cmd);

	RTL_W16(IntrMitigate, 0x0000);

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);
	rtl_set_rx_tx_config_registers(tp);

	RTL_W8(Cfg9346, Cfg9346_Lock);

	RTL_R8(IntrMask);

	rtl_set_rx_mode(dev);

	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xf000);

	//        RTL_W16(IntrMask, tp->intr_event);
}

/*** Etherboot API Support Routines ***/

/**
    Because Etherboot's pci_device_id structure does not contain a
    field to contain arbitrary data, we need the following table to
    associate PCI IDs with nic variants, because a lot of driver
    routines depend on knowing which kind of variant they are dealing
    with. --mdc
 **/

#define _R(VENDOR,DEVICE,INDEX) \
	{ .vendor = VENDOR, .device = DEVICE, .index = INDEX }

static const struct {
	uint16_t vendor;
	uint16_t device;
	int index;
} nic_variant_table[] = {
	_R(0x10ec, 0x8129, RTL_CFG_0),
	_R(0x10ec, 0x8136, RTL_CFG_2),
	_R(0x10ec, 0x8167, RTL_CFG_0),
	_R(0x10ec, 0x8168, RTL_CFG_1),
	_R(0x10ec, 0x8169, RTL_CFG_0),
	_R(0x1186, 0x4300, RTL_CFG_0),
	_R(0x1259, 0xc107, RTL_CFG_0),
	_R(0x16ec, 0x0116, RTL_CFG_0),
	_R(0x1737, 0x1032, RTL_CFG_0),
	_R(0x0001, 0x8168, RTL_CFG_2),
};
#undef _R

static int
rtl8169_get_nic_variant ( uint16_t vendor, uint16_t device )
{
	u32 i;

	DBGP ( "rtl8169_get_nic_variant\n" );

	for (i = 0; i < ARRAY_SIZE(nic_variant_table); i++) {
		if ( ( nic_variant_table[i].vendor == vendor ) &&
		     ( nic_variant_table[i].device == device ) ) {
			return ( nic_variant_table[i].index );
		}
	}
	DBG ( "No matching NIC variant found!\n" );
	return ( RTL_CFG_0 );
}

static void
r8169_init_rings ()
{
	int i;
	u8 diff;
	u32 TxPhysAddr, RxPhysAddr;

	DBGP ( "r8169_init_ring\n" );

	tp->cur_tx = 0;
	tp->cur_rx = 0;

	TxPhysAddr = virt_to_bus ( tx_ring );
	diff = 256 - ( TxPhysAddr - ( ( TxPhysAddr >> 8 ) << 8 ) );
	tp->TxDescArray = (struct TxDesc *) ( &tx_ring[0] + diff );

	RxPhysAddr = virt_to_bus ( rx_ring );
	diff = 256 - ( RxPhysAddr - ( ( RxPhysAddr >> 8 ) << 8 ) );
	tp->RxDescArray = (struct RxDesc *) ( &rx_ring[0] + diff );

	memset(tp->TxDescArray, 0, NUM_TX_DESC * sizeof ( struct TxDesc ) );
	memset(tp->RxDescArray, 0, NUM_RX_DESC * sizeof ( struct RxDesc ) );

        tp->TxDescArray[NUM_TX_DESC - 1].status = EORbit;

	for (i = 0; i < NUM_RX_DESC; i++) {

		tp->RxBufferRing[i] = &rxb[i * RX_BUF_SIZE];

		tp->RxDescArray[i].buf_Haddr = 0;
		tp->RxDescArray[i].buf_addr =
		    virt_to_bus ( tp->RxBufferRing[i] );
                tp->RxDescArray[i].vlan_tag = 0;
                tp->RxDescArray[i].status =
                        ( ( i == ( NUM_RX_DESC - 1 ) ) ? EORbit : 0 ) +
                        RX_BUF_SIZE;
                tp->RxDescArray[i].status |= OWNbit;
	}
}

/*** Etherboot Core API Routines ***/

static void
r8169_irq ( struct nic *nic __unused, irq_action_t action )
{
	uint16_t intr_status = 0;
	uint32_t interested = RxOverflow | RxFIFOOver | RxErr | RxOK;

	DBGP ( "r8169_irq\n" );

	switch ( action ) {
	case DISABLE:
	case ENABLE:

		intr_status = RTL_R16 ( IntrStatus );

		/* if h/w no longer present (hotplug?) or major error, 
		   just bail out */
		if ( intr_status == 0xFFFF )
			break;

		intr_status = intr_status & ~interested;

		if ( action == ENABLE )
			intr_status = intr_status | interested;

		RTL_W16 ( IntrMask, intr_status );
		break;

	case FORCE:

		RTL_W8 ( TxPoll, ( RTL_R8 ( TxPoll ) | 0x01 ) );
		break;
	}
}

/**
 * poll - check for packet and retrieve if requested
 *
 * Return 1 if there is a packet ready 0 otherwise
 *
 * If parameter "retrieve" is zero and there is a packet
 *    available, return 1, but do not consume the packet
 *
 * If there is a packet ready:
 *     nic->packet should contain data on return
 *     nic->packetlen should contain length of data
 **/
static int 
r8169_poll ( struct nic *nic, int retrieve )
{
        unsigned int intr_status = RTL_R16 ( IntrStatus );
        unsigned int cur_rx = tp->cur_rx;
        int rc = 0;

	DBGP ( "r8169_poll\n" );

        /* Acknowledge all possible interrupts */
	RTL_W16 ( IntrStatus, 0xFFFF );

        /* h/w no longer present (hotplug?) or major error, bail */
        if ( intr_status == 0xFFFF ) {
                printf ( "Hotplug error during poll!\n" );
                return rc;
        }

	if ( ( tp->RxDescArray[cur_rx].status & OWNbit ) == 0 ) {

		/* There is a packet ready */
                rc = 1;

                /**
                 * If we are not supposed to retrieve the packet,
                 * just return, but indicate a packet is present 
                 **/
		if ( retrieve == 0 )
			return rc;

                /* Packet received without error */
		if ( ! (tp->RxDescArray[cur_rx].status & RxRES ) ) {

			nic->packetlen = (int) ( tp->RxDescArray[cur_rx].
						status & 0x00001FFF ) - 4;
			memcpy ( nic->packet, tp->RxBufferRing[cur_rx],
			       nic->packetlen );
                        DBG ( "RX %d bytes\n", nic->packetlen );

		} else {

                        /* Some kind of receive error occurred */
			printf ( "RX Error!\n" );

                        /* Indicate that no valid packet was received */
                        rc = 0;
                }

                /* Prepare descriptor for next packet */

                tp->RxDescArray[cur_rx].vlan_tag = 0;

                tp->RxDescArray[cur_rx].buf_Haddr = 0;

		tp->RxBufferRing[cur_rx] = &rxb[cur_rx * RX_BUF_SIZE];
                tp->RxDescArray[cur_rx].buf_addr =
                        virt_to_bus ( tp->RxBufferRing[cur_rx] );

                tp->RxDescArray[cur_rx].status =
                        ( ( cur_rx == ( NUM_RX_DESC - 1 ) ) ? EORbit : 0 ) |
                        RX_BUF_SIZE;

                tp->RxDescArray[cur_rx].status |= OWNbit;

		tp->cur_rx = (cur_rx + 1) % NUM_RX_DESC;

	}
	return rc;
}

static void 
r8169_transmit ( struct nic *nic, 
                 const char *d,	   /* Destination */
                 unsigned int t,   /* Type */
                 unsigned int s,   /* size */
                 const char *p )   /* Packet */
{
	u16 nstype;
	u32 to;
	unsigned int  entry = tp->cur_tx;
	unsigned char *ptxb = &txb[tp->cur_tx * TX_BUF_SIZE];

	DBGP ( "r8169_transmit\n" );

	memcpy ( ptxb, d, ETH_ALEN );
	memcpy ( ptxb + ETH_ALEN, nic->node_addr, ETH_ALEN );
	nstype = htons ( (u16) t );
	memcpy ( ptxb + 2 * ETH_ALEN, (u8 *) &nstype, 2 );
	memcpy ( ptxb + ETH_HLEN, p, s );

	s += ETH_HLEN;
	s &= 0x0FFF;

        /* Pad short packets */
        while ( s < ETH_ZLEN )
                ptxb[s++] = '\0';

	tp->TxDescArray[entry].buf_Haddr = 0;
	tp->TxDescArray[entry].buf_addr  = virt_to_bus ( ptxb );
        tp->TxDescArray[entry].vlan_tag = 0;
        tp->TxDescArray[entry].status =
                ( ( entry == ( NUM_TX_DESC - 1 ) ? EORbit : 0 ) |
                  FSbit | LSbit ) | s;

        tp->TxDescArray[entry].status |= OWNbit;

	tp->cur_tx = ( tp->cur_tx + 1 ) % NUM_TX_DESC;

        /* Force NIC to check transmit queue */
	RTL_W8 ( TxPoll, NPQ );

	to = currticks() + TX_TIMEOUT;
	while ( ( tp->TxDescArray[entry].status & OWNbit) && 
                ( currticks() < to ) ) {
                RTL_R8 ( ChipCmd );   /* wait */
        }

	if ( currticks () >= to ) {
		DBG ( "TX Time Out!\n" );
                return;
	}
        DBG ( "TX %d bytes\n", s );
}

static void
r8169_disable ( struct dev *dev __unused )
{
	int i;

	DBGP ( "r8169_disable\n" );

	/* Stop the chip's Tx and Rx DMA processes. */
	RTL_W8 ( ChipCmd, 0x00 );

	/* Disable interrupts by clearing the interrupt mask. */
	RTL_W16 ( IntrMask, 0x0000 );

	RTL_W32 ( RxMissed, 0 );

        rtl8169_hw_reset ( ioaddr );

	tp->TxDescArray  = NULL;
	tp->RxDescArray  = NULL;

	for ( i = 0; i < NUM_RX_DESC; i++ ) {
		tp->RxBufferRing[i] = NULL;
	}
}

static int
r8169_probe ( struct dev *dev, struct pci_device *pdev )
{
	struct nic *nic = (struct nic *) dev;
	int i;

	int cfg_index = rtl8169_get_nic_variant ( pdev->vendor, pdev->dev_id );
	const struct rtl_cfg_info *cfg = rtl_cfg_infos + cfg_index;

	DBGP ( "r8169_probe\n" );

        printf ( "\nTest r8169 etherboot driver.\n" );
	printf ( "%s: Found %s, Vendor=%hX Device=%hX\n",
	       pdev->name, pdev->name, pdev->vendor, pdev->dev_id );

	DBG ( "cfg_index = %d\n", cfg_index );
	DBG ( "cfg->intr_event = %#04x\n", cfg->intr_event );

	/* point to private storage */
	tp = &tpx;

	memset ( tp, 0, ( sizeof ( *tp ) ) );

	tp->pci_dev    = pdev;
	tp->irqno      = pdev->irq;
	tp->cfg_index  = cfg_index;
	tp->intr_event = cfg->intr_event;
	tp->cp_cmd     = PCIMulRW;

	tp->hw_start   = cfg->hw_start;

	ioaddr = pci_bar_start ( pdev, PCI_BASE_ADDRESS_0 );

	tp->pcie_cap = pci_find_capability ( pdev, PCI_CAP_ID_EXP );
	if ( tp->pcie_cap ) {
		DBG (  "PCI Express capability\n" );
	} else {
		DBG (  "No PCI Express capability\n" );
	}

	adjust_pci_device ( pdev );

	/* Mask interrupts just in case */
	rtl8169_irq_mask_and_ack ( ioaddr );

        rtl_soft_reset ( dev );

	rtl8169_get_mac_version ( tp, ioaddr );

	/* Identify chipset */
	rtl_get_chipset ();

	for ( i = 0; i < MAC_ADDR_LEN; i++ )
		nic->node_addr[i] = RTL_R8 ( MAC0 + i );

	printf ( "%s: %! at ioaddr %hX\n", pdev->name, nic->node_addr,
	       ioaddr );

        rtl_soft_reset ( dev );

        /* Autonegotiation may take 5 seconds, and phy initialization
           may not be necessary, so comment out for now */
        if ( 0 ) {
                rtl8169_init_phy ( dev, tp );
                mdelay ( 5000 );
        }

        r8169_init_rings ();

	rtl_hw_start ( dev );

	nic->irqno = pdev->irq;
	nic->ioaddr = ioaddr;

	nic->transmit = r8169_transmit;
	nic->poll = r8169_poll;
	nic->irq = r8169_irq;
	dev->disable = r8169_disable;

	return 1;
}

static struct pci_id r8169_nics[] = {
	PCI_ROM(0x10ec, 0x8129, "rtl8169-0x8129", "rtl8169-0x8129"),
	PCI_ROM(0x10ec, 0x8136, "rtl8169-0x8136", "rtl8169-0x8136"),
	PCI_ROM(0x10ec, 0x8167, "rtl8169-0x8167", "rtl8169-0x8167"),
	PCI_ROM(0x10ec, 0x8168, "rtl8169-0x8168", "rtl8169-0x8168"),
	PCI_ROM(0x10ec, 0x8169, "rtl8169-0x8169", "rtl8169-0x8169"),
	PCI_ROM(0x1186, 0x4300, "rtl8169-0x4300", "rtl8169-0x4300"),
	PCI_ROM(0x1259, 0xc107, "rtl8169-0xc107", "rtl8169-0xc107"),
	PCI_ROM(0x16ec, 0x0116, "rtl8169-0x0116", "rtl8169-0x0116"),
	PCI_ROM(0x1737, 0x1032, "rtl8169-0x1032", "rtl8169-0x1032"),
	PCI_ROM(0x0001, 0x8168, "rtl8169-0x8168", "rtl8169-0x8168"),
};

static struct pci_driver r8169_driver __pci_driver = {
	.type = NIC_DRIVER,
	.name = "r8169/PCI",
	.probe = r8169_probe,
	.ids = r8169_nics,
	.id_count = sizeof(r8169_nics) / sizeof(r8169_nics[0]),
	.class = 0,
};

/*
 * Local variables:
 *  c-basic-offset: 8
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
