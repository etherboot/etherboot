/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Inter Pro 1000 for Etherboot
Drivers are port from Intel's Linux driver e1000-3.1.23

***************************************************************************/
/*****************************************************************************
 *****************************************************************************

 Copyright (c) 1999 - 2001, Intel Corporation 

 All rights reserved.

 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, 
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation 
     and/or other materials provided with the distribution.

  3. Neither the name of Intel Corporation nor the names of its contributors 
     may be used to endorse or promote products derived from this software 
     without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

 *****************************************************************************
 *****************************************************************************/

/*
 *  Copyright (C) Archway Digital Solutions.
 *
 *  written by Chrsitopher Li <cli at arcyway dot com> or <chrisl at gnuchina dot org>
 *  2/9/2002
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the PCI support functions, if this is a PCI NIC */
#include "pci.h"
/* to get our own prototype */
#include "cards.h"
#include "timer.h"

#undef	virt_to_bus
#define	virt_to_bus(x)	((unsigned long)x)

typedef unsigned char *dma_addr_t;
typedef unsigned char u8, uint8_t;
typedef signed char s8;
typedef unsigned short u16, uint16_t;
typedef signed short s16;
typedef unsigned int u32, uint32_t;
typedef signed int s32, atomic_t;

typedef unsigned long long u64, uint64_t;

/*
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the CRIS architecture, we just read/write the
 * memory location directly.
 */
#define readb(addr) (*(volatile unsigned char *) (addr))
#define readw(addr) (*(volatile unsigned short *) (addr))
#define readl(addr) (*(volatile unsigned int *) (addr))

#define writeb(b,addr) ((*(volatile unsigned char *) (addr)) = (b))
#define writew(b,addr) ((*(volatile unsigned short *) (addr)) = (b))
#define writel(b,addr) ((*(volatile unsigned int *) (addr)) = (b))

// #define DEBUG 1
#include "e1000.h"
#include "e1000_fxhw.h"
#include "e1000_phy.h"
#define udelay(n)       waiton_timer2(((n)*TICKS_PER_MS)/1000)
#define mdelay(n)       waiton_timer2(((n)*TICKS_PER_MS))

struct pci_device *pci_dev;
u8 mac_addr[ETH_LENGTH_OF_ADDRESS];
unsigned int ioaddr;
u8 MacType;
u8 MediaType;
u8 ReportTxEarly;
u8 TbiCompatibilityEnable = TRUE;

u16 FlowControlHighWatermark = FC_DEFAULT_HI_THRESH;
u16 FlowControlLowWatermark = FC_DEFAULT_LO_THRESH;
u16 FlowControlPauseTime = FC_DEFAULT_TX_TIMER;
u8 FlowControlSendXon = TRUE;
u8 FlowControl = FLOW_CONTROL_NONE;
u8 OriginalFlowControl = FLOW_CONTROL_NONE;
u16 PciCommandWord;
u8 AutoNegFailed = 0;
u8 AutoNeg = 1;
u8 AutoNegAdvertised;
u32 PhyId;
u32 PhyAddress;
u8 WaitAutoNegComplete = 1;
u32 TxcwRegValue;
u8 GetLinkStatus = 1;
u8 TbiCompatibilityOn;
u8 AdapterStopped;

char tx_pool[128 + 16];
char rx_pool[128 + 16];
char packet[2096];

E1000_TRANSMIT_DESCRIPTOR *tx_base;
E1000_RECEIVE_DESCRIPTOR *rx_base;

int tx_head, tx_tail;
int rx_head, rx_tail, rx_last;

#include "e1000_phy.c"
#include "e1000_fxhw.c"


/*********************************************************************/
/*! @brief  Reads the MAC address from the EEPROM
 *  @param  Adapter board private structure
 *  @param  NodeAddress pointer to an array of bytes
 *********************************************************************/

static void
ReadNodeAddress (uint8_t * NodeAddress)
{
	uint16_t EepromWordValue;
	int i;

	E1000_DBG ("ReadNodeAddress\n");

	for (i = 0; i < NODE_ADDRESS_SIZE; i += 2) {
		EepromWordValue = ReadEepromWord (EEPROM_NODE_ADDRESS_BYTE_0 + (i / 2));
		NodeAddress[i] = (uint8_t) (EepromWordValue & 0x00FF);
		NodeAddress[i + 1] = (uint8_t) (EepromWordValue >> 8);
	}

	return;
}

void
fill_rx (void)
{
	E1000_RECEIVE_DESCRIPTOR *rd;
	rx_last = rx_tail;
	rd = rx_base + rx_tail;
	rx_tail = (rx_tail + 1) % 8;
	memset (rd, 0, 16);
	rd->BufferAddress.Lo32 = (u32) packet;
	E1000_WRITE_REG (Rdt0, rx_tail);
}

void
init_descriptor (void)
{
	u32 p;
	u32 tctl, reg_tipg, tmp;
	p = (u32) tx_pool;
	if (p & 0xf)
		p = (p + 0x10) & (~0xf);

	tx_base = (PE1000_TRANSMIT_DESCRIPTOR) p;

	E1000_WRITE_REG (Tdbal, (u32) tx_base);
	E1000_WRITE_REG (Tdbah, 0);
	E1000_WRITE_REG (Tdl, 128);
	/* Setup the HW Tx Head and Tail descriptor pointers */

	E1000_WRITE_REG (Tdh, 0);
	E1000_WRITE_REG (Tdt, 0);
	tx_tail = 0;

	tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
	    (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT) | (E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);
	E1000_WRITE_REG (Tctl, tctl);

	rx_tail = 0;
	/* disable receive */
	E1000_WRITE_REG (Rctl, 0);
	p = (u32) rx_pool;
	if (p & 0xf)
		p = (p + 0x10) & (~0xf);
	rx_base = (E1000_RECEIVE_DESCRIPTOR *) p;
	E1000_WRITE_REG (Rdbal0, (u32) rx_base);
	E1000_WRITE_REG (Rdbah0, 0);

	E1000_WRITE_REG (Rdlen0, 128);
	/* Setup the HW Rx Head and Tail Descriptor Pointers */
	E1000_WRITE_REG (Rdh0, 0);
	E1000_WRITE_REG (Rdt0, 0);

	E1000_WRITE_REG (Rctl, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048);
	fill_rx ();
}

/* NIC specific static variables go here */

/**************************************************************************
RESET - Reset adapter
***************************************************************************/
static void
e1000_reset (struct nic *nic)
{
	/* put the card in its initial state */
	E1000_WRITE_REG (Ctrl, E1000_CTRL_RST);
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int
e1000_poll (struct nic *nic)
{
	/* return true if there's an ethernet packet ready to read */
	/* nic->packet should contain data on return */
	/* nic->packetlen should contain length of data */
	E1000_RECEIVE_DESCRIPTOR *rd;
	rd = rx_base + rx_last;
	if (!rd->ReceiveStatus & E1000_RXD_STAT_DD)
		return 0;
	//      printf("recv: packet %! -> %! len=%d \n", packet+6, packet,rd->Length);
	memcpy (nic->packet, packet, rd->Length);
	nic->packetlen = rd->Length;
	fill_rx ();
	return 1;
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void
e1000_transmit (struct nic *nic, const char *d,	/* Destination */
		    unsigned int t,	/* Type */
		    unsigned int s,	/* size */
		    const char *p)
{				/* Packet */
	/* send the packet to destination */
	struct eth_hdr {
		unsigned char dst_addr[ETH_ALEN];
		unsigned char src_addr[ETH_ALEN];
		unsigned short type;
	} hdr;
	unsigned char tx_buffer[1680];
	unsigned short status;
	int to;
	int s1, s2;
	int i;
	PE1000_TRANSMIT_DESCRIPTOR txhd, txp;
	DEBUGOUT("send\n");	
	memcpy (&hdr.dst_addr, d, ETH_ALEN);
	memcpy (&hdr.src_addr, nic->node_addr, ETH_ALEN);

	hdr.type = htons (t);
	txhd = tx_base + tx_tail;
	tx_tail = (tx_tail + 1) % 8;
	txp = tx_base + tx_tail;
	tx_tail = (tx_tail + 1) % 8;

	txhd->BufferAddress.Lo32 = virt_to_bus (&hdr);
	txhd->BufferAddress.Hi32 = 0;
	txhd->Lower.DwordData = sizeof (hdr);
	txhd->Upper.DwordData = 0;

	txp->BufferAddress.Lo32 = virt_to_bus (p);
	txp->BufferAddress.Hi32 = 0;
	txp->Lower.DwordData = E1000_TXD_CMD_RPS | E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | s;
	txp->Upper.DwordData = 0;

	E1000_WRITE_REG (Tdt, tx_tail);
	while (!(txp->Upper.DwordData & E1000_TXD_STAT_DD)) {
		udelay (10);	/* give the nic a chance to write to the register */
	}
	DEBUGOUT("send end\n");
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void
e1000_disable (struct nic *nic)
{
	E1000_WRITE_REG (Rctl, 0);
	E1000_WRITE_REG (Tctl, 0);
	mdelay (10);

}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
You should omit the last argument struct pci_device * for a non-PCI NIC
***************************************************************************/
struct nic *
e1000_probe (struct nic *nic, unsigned short *probe_addrs, struct pci_device *p)
{
	unsigned short pci_command;
	unsigned short new_command;
	unsigned char pci_latency;
	u32 dev_state;
	u8 revision;
	int i;

	if (p == 0 || p->ioaddr == 0)
		return 0;
	pci_dev = p;
	ioaddr = p->ioaddr & ~3;	/* Mask the bit that says "this is an io addr" */
	/* From Matt Hortman <mbhortman@acpthinclient.com> */
	/* MAC and Phy settings */

	switch (p->dev_id) {
	case PCI_DEVICE_ID_82542:
		pcibios_read_config_byte (p->bus, p->devfn, PCI_REVISION, &revision);
		switch (revision) {
		case WISEMAN_2_0_REV_ID:
			MacType = MAC_WISEMAN_2_0;
			break;
		case WISEMAN_2_1_REV_ID:
			MacType = MAC_WISEMAN_2_1;
			break;
		default:
			MacType = MAC_WISEMAN_2_0;
			E1000_ERR ("Could not identify 82542 revision\n");
		}
		break;
	case PCI_DEVICE_ID_82543GC_FIBER:
	case PCI_DEVICE_ID_82543GC_COPPER:
		MacType = MAC_LIVENGOOD;
		break;
	case PCI_DEVICE_ID_82544EI_COPPER:
	case PCI_DEVICE_ID_82544GC_CREB:
		MacType = MAC_CORDOVA;
		break;
	default:
		E1000_ERR ("Could not identify hardware\n");
		return 0;
	}
	if (MacType != MACTYPE) {
		printf("Please add -DMACTYPE=%d and rebuild the driver\n",MacType);
		return 0;
	}

	/* Identify the Hardware - this is done by the gigabit shared code
	 * in InitializeHardware, but it would help to identify the NIC
	 * before bringing the hardware online for use in CheckOptions. */

	if ((E1000_REPORT_TX_EARLY == 0) || (E1000_REPORT_TX_EARLY == 1)) {
		ReportTxEarly = E1000_REPORT_TX_EARLY;
	} else {
#if MACTYPE < MAC_LIVENGOOD
			ReportTxEarly = 0;
#else 
			ReportTxEarly = 1;
#endif
	}
	/* SoftwareInit() ends here */

	/*
	   * check to make sure the bios properly set the
	   * 82557 (or 82558) to be bus master
	   *
	   * from eepro100.c in 2.2.9 kernel source
	 */

	pcibios_read_config_word (p->bus, p->devfn, PCI_COMMAND, &PciCommandWord);
	new_command = PciCommandWord | PCI_COMMAND_MASTER | PCI_COMMAND_MEM;

	if (PciCommandWord != new_command) {
		printf ("\nThe PCI BIOS has not enabled this device!\n"
			"Updating PCI command %hX->%hX. pci_bus %hhX pci_device_fn %hhX\n",
			PciCommandWord, new_command, p->bus, p->devfn);
		pcibios_write_config_word (p->bus, p->devfn, PCI_COMMAND, new_command);
		PciCommandWord = new_command;
	}
	dev_state = E1000_READ_REG (Status);
#ifdef EEPROM_CHECKSUM
	if (!ValidateEepromChecksum ()) {
		printf ("The EEPROM Checksum Is Not Valid\n");
		return 0;
	}
#endif
	ReadNodeAddress (mac_addr);
	memcpy (nic->node_addr, mac_addr, ETH_ALEN);
	
	printf("MAC address %!\n",nic);

	if (!InitializeHardware ()) {
		E1000_ERR ("Hardware Initialization Failed\n");
		return 0;
	}

	// CheckForLink ();
	/* disable all the interrupt */
	E1000_WRITE_REG (Imc, 0xffffffff);

	init_descriptor ();
	/* if board found */
	{
		/* point to NIC specific routines */
		nic->reset = e1000_reset;
		nic->poll = e1000_poll;
		nic->transmit = e1000_transmit;
		nic->disable = e1000_disable;
		return nic;
	}
	/* else */
	return 0;
}
