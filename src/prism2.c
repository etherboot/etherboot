/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Prism2 NIC driver for Etherboot

Written by Michael Brown of Fen Systems Ltd
$Id$
***************************************************************************/

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

/*
 * Hard-coded SSID
 * Leave blank in order to connect to any available SSID
 */

static const char hardcoded_ssid[] = "";

/*
 * Type of Prism2 interface to support
 * If not already defined, select PLX
 */
#ifndef WLAN_HOSTIF
#define WLAN_HOSTIF WLAN_PLX
#endif

/*
 * Include wlan_compat, p80211 and hfa384x header files from Linux Prism2 driver
 * We need to hack some defines in order to avoid compiling kernel-specific routines
 */

#define __LINUX_WLAN__
#undef __KERNEL__
#define __I386__
#include "wlan_compat.h"
#include "p80211hdr.h"
#include "hfa384x.h"
#define BAP_TIMEOUT ( 5000 )

/*
 * A few hacks to make the coding environment more Linux-like.  This makes it somewhat
 * quicker to convert code from the Linux Prism2 driver.
 */
#include <errno.h>
#include "timer.h"
#define __le16_to_cpu(x) (x)
#define __le32_to_cpu(x) (x)
#define __cpu_to_le16(x) (x)
#define __cpu_to_le32(x) (x)

/*
 * PLX9052 PCI register offsets
 * Taken from PLX9052 datasheet available from http://www.plxtech.com/download/9052/databook/9052db-20.pdf
 */

#define PLX_LOCAL_CONFIG_REGISTER_BASE ( PCI_BASE_ADDRESS_1 )
#define PLX_LOCAL_ADDRESS_SPACE_0_BASE ( PCI_BASE_ADDRESS_2 )
#define PLX_LOCAL_ADDRESS_SPACE_1_BASE ( PCI_BASE_ADDRESS_3 )
#define PLX_LOCAL_ADDRESS_SPACE_2_BASE ( PCI_BASE_ADDRESS_4 )
#define PLX_LOCAL_ADDRESS_SPACE_3_BASE ( PCI_BASE_ADDRESS_5 )

#define PRISM2_PLX_ATTR_MEM_BASE       ( PLX_LOCAL_ADDRESS_SPACE_0_BASE )
#define PRISM2_PLX_IO_BASE             ( PLX_LOCAL_ADDRESS_SPACE_1_BASE )

#define PRISM2_PCI_MEM_BASE            ( PCI_BASE_ADDRESS_0 )

/*
 * PCMCIA CIS types
 * Taken from cistpl.h in pcmcia-cs
 */

#define CISTPL_VERS_1           ( 0x15 )
#define CISTPL_END              ( 0xff )

#define CIS_STEP                ( 2 )
#define CISTPL_HEADER_LEN       ( 2 * CIS_STEP )
#define CISTPL_LEN_OFF          ( 1 * CIS_STEP )
#define CISTPL_VERS_1_STR_OFF   ( 4 * CIS_STEP )

/*
 * Prism2 constants
 * Taken from prism2sta.c in linux-wlan-ng
 */

#define COR_OFFSET      ( 0x3e0 )   /* COR attribute offset of Prism2 PC card */
#define COR_VALUE       ( 0x41 )    /* Enable PC card with irq in level trigger (but interrupts disabled) */

/* NIC specific static variables */

/* The hfa384x_t structure is used extensively in the Linux driver but is ifdef'd out in our include since __KERNEL__ is not defined.
 * This is a dummy version that contains only the fields we are interested in.
 */

typedef struct hfa384x
{
  UINT32 iobase;
  UINT32 membase;
  UINT16 lastcmd;
  UINT16 status;         /* in host order */
  UINT16 resp0;          /* in host order */
  UINT16 resp1;          /* in host order */
  UINT16 resp2;          /* in host order */
  UINT8  bssid[WLAN_BSSID_LEN];
} hfa384x_t;

/* The global instance of the hardware (i.e. where we store iobase and membase, in the absence of anywhere better to put them */
static hfa384x_t hw_global = {
  0, 0, 0, 0, 0, 0, 0, {0,0,0,0,0,0}
};

/*
 * 802.11 headers in addition to those in hfa384x_tx_frame_t (LLC and SNAP)
 * Taken from p80211conv.h
 */

typedef struct wlan_llc
{
  UINT8   dsap                            __WLAN_ATTRIB_PACK__;
  UINT8   ssap                            __WLAN_ATTRIB_PACK__;
  UINT8   ctl                             __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ wlan_llc_t;

static const wlan_llc_t wlan_llc_snap = { 0xaa, 0xaa, 0x03 }; /* LLC header indicating SNAP (?) */

#define WLAN_IEEE_OUI_LEN 3
typedef struct wlan_snap
{
  UINT8   oui[WLAN_IEEE_OUI_LEN]          __WLAN_ATTRIB_PACK__;
  UINT16  type                            __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ wlan_snap_t;

typedef struct wlan_80211hdr
{
  wlan_llc_t llc;
  wlan_snap_t snap;
} wlan_80211hdr_t;

/*
 * Function prototypes
 */

static int prism2_find_plx ( hfa384x_t *hw, struct pci_device *p );
static int prism2_find_pci ( hfa384x_t *hw, struct pci_device *p );

/*
 * Hardware-level hfa384x functions
 * These are based on the ones in hfa384x.h (which are ifdef'd out since __KERNEL__ is not defined).
 * Basically, these functions are the result of hand-evaluating all the ifdefs and defines in the hfa384x.h versions. 
 */

/* Retrieve the value of one of the MAC registers. */
static inline UINT16 hfa384x_getreg( hfa384x_t *hw, UINT reg )
{
#if (WLAN_HOSTIF == WLAN_PLX)
  return inw ( hw->iobase + reg );
#elif (WLAN_HOSTIF == WLAN_PCI)
  return readw ( hw->membase + reg );
#endif
}

/* Set the value of one of the MAC registers. */
static inline void hfa384x_setreg( hfa384x_t *hw, UINT16 val, UINT reg )
{
#if (WLAN_HOSTIF == WLAN_PLX)
  outw ( val, hw->iobase + reg );
#elif (WLAN_HOSTIF == WLAN_PCI)
  writew ( val, hw->membase + reg );
#endif
  return;
}

/* 
 * Noswap versions
 * Etherboot is i386 only, so swap and noswap are the same...
 */
static inline UINT16 hfa384x_getreg_noswap( hfa384x_t *hw, UINT reg )
{
  return hfa384x_getreg ( hw, reg );
}
static inline void hfa384x_setreg_noswap( hfa384x_t *hw, UINT16 val, UINT reg )
{
  hfa384x_setreg ( hw, val, reg );
}

/*
 * Low-level hfa384x functions
 * These are based on the ones in hfa384x.c, modified to work in the Etherboot environment.
 */

/*
 * hfa384x_docmd_wait
 *
 * Waits for availability of the Command register, then
 * issues the given command.  Then polls the Evstat register
 * waiting for command completion.
 * Arguments:
 *       hw              device structure
 *       cmd             Command in host order
 *       parm0           Parameter0 in host order
 *       parm1           Parameter1 in host order
 *       parm2           Parameter2 in host order
 * Returns:
 *       0               success
 *       >0              command indicated error, Status and Resp0-2 are
 *                       in hw structure.
 */
static int hfa384x_docmd_wait( hfa384x_t *hw, UINT16 cmd, UINT16 parm0, UINT16 parm1, UINT16 parm2)
{
  UINT16 reg = 0;
  UINT16 counter = 0;
  
  /* wait for the busy bit to clear */	
  counter = 0;
  reg = hfa384x_getreg(hw, HFA384x_CMD);
  while ( HFA384x_CMD_ISBUSY(reg) && (counter < 10) ) {
    reg = hfa384x_getreg(hw, HFA384x_CMD);
    counter++;
    udelay(10);
  }
  if (HFA384x_CMD_ISBUSY(reg)) {
    printf("hfa384x_cmd timeout(1), reg=0x%0hx.\n", reg);
    return -ETIMEDOUT;
  }

  /* busy bit clear, write command */
  hfa384x_setreg(hw, parm0, HFA384x_PARAM0);
  hfa384x_setreg(hw, parm1, HFA384x_PARAM1);
  hfa384x_setreg(hw, parm2, HFA384x_PARAM2);
  hw->lastcmd = cmd;
  hfa384x_setreg(hw, cmd, HFA384x_CMD);
  
  /* Now wait for completion */
  counter = 0;
  reg = hfa384x_getreg(hw, HFA384x_EVSTAT);
  /* Initialization is the problem.  It takes about
     100ms. "normal" commands are typically is about
     200-400 us (I've never seen less than 200).  Longer
     is better so that we're not hammering the bus. */
  while ( !HFA384x_EVSTAT_ISCMD(reg) && (counter < 5000)) {
    reg = hfa384x_getreg(hw, HFA384x_EVSTAT);
    counter++;
    udelay(200);
  }
  if ( ! HFA384x_EVSTAT_ISCMD(reg) ) {
    printf("hfa384x_cmd timeout(2), reg=0x%0hx.\n", reg);
    return -ETIMEDOUT;
  }

  /* Read status and response */
  hw->status = hfa384x_getreg(hw, HFA384x_STATUS);
  hw->resp0 = hfa384x_getreg(hw, HFA384x_RESP0);
  hw->resp1 = hfa384x_getreg(hw, HFA384x_RESP1);
  hw->resp2 = hfa384x_getreg(hw, HFA384x_RESP2);
  hfa384x_setreg(hw, HFA384x_EVACK_CMD, HFA384x_EVACK);
  return HFA384x_STATUS_RESULT_GET(hw->status);
}

/*
 * Prepare BAP for access.  Assigns FID and RID, sets offset register
 * and waits for BAP to become available.
 *
 * Arguments:
 *	hw		device structure
 *	id		FID or RID, destined for the select register (host order)
 *	offset		An _even_ offset into the buffer for the given FID/RID.
 * Returns: 
 *	0		success
 */
static int hfa384x_prepare_bap(hfa384x_t *hw, UINT16 id, UINT16 offset)
{
  int result = 0;
  UINT16 reg;
  UINT16 i;

  /* Validate offset, buf, and len */
  if ( (offset > HFA384x_BAP_OFFSET_MAX) || (offset % 2) ) {
    result = -EINVAL;
  } else {
    /* Write fid/rid and offset */
    hfa384x_setreg(hw, id, HFA384x_SELECT0);
    udelay(10);
    hfa384x_setreg(hw, offset, HFA384x_OFFSET0);
    /* Wait for offset[busy] to clear (see BAP_TIMEOUT) */
    i = 0; 
    do {
      reg = hfa384x_getreg(hw, HFA384x_OFFSET0);
      if ( i > 0 ) udelay(2);
      i++;
    } while ( i < BAP_TIMEOUT && HFA384x_OFFSET_ISBUSY(reg));
    if ( i >= BAP_TIMEOUT ) {
      /* failure */
      result = reg;
    } else if ( HFA384x_OFFSET_ISERR(reg) ){
      /* failure */
      result = reg;
    }
  }
  return result;
}

/*
 * Copy data from BAP to memory.
 *
 * Arguments:
 *	hw		device structure
 *	id		FID or RID, destined for the select register (host order)
 *	offset		An _even_ offset into the buffer for the given FID/RID.
 *	buf		ptr to array of bytes
 *	len		length of data to transfer in bytes
 * Returns: 
 *	0		success
 */
static int hfa384x_copy_from_bap(hfa384x_t *hw, UINT16 id, UINT16 offset,
			  void *buf, UINT len)
{
  int result = 0;
  UINT8	*d = (UINT8*)buf;
  UINT16 i;
  UINT16 reg = 0;
  
  /* Prepare BAP */
  result = hfa384x_prepare_bap ( hw, id, offset );
  if ( result == 0 ) {
    /* Read even(len) buf contents from data reg */
    for ( i = 0; i < (len & 0xfffe); i+=2 ) {
      *(UINT16*)(&(d[i])) = hfa384x_getreg_noswap(hw, HFA384x_DATA0);
    }
    /* If len odd, handle last byte */
    if ( len % 2 ){
      reg = hfa384x_getreg_noswap(hw, HFA384x_DATA0);
      d[len-1] = ((UINT8*)(&reg))[0];
    }
  }
  if (result) {
    printf ( "copy_from_bap(%#hx, %#hx, %d) failed, result=%#hx\n", id, offset, len, result);
  }
  return result;
}

/*
 * Copy data from memory to BAP.
 *
 * Arguments:
 *	hw		device structure
 *	id		FID or RID, destined for the select register (host order)
 *	offset		An _even_ offset into the buffer for the given FID/RID.
 *	buf		ptr to array of bytes
 *	len		length of data to transfer in bytes
 * Returns: 
 *	0		success
 */
static int hfa384x_copy_to_bap(hfa384x_t *hw, UINT16 id, UINT16 offset,
			void *buf, UINT len)
{
  int result = 0;
  UINT8	*d = (UINT8*)buf;
  UINT16 i;
  UINT16 reg;
  UINT16 savereg;

  /* Prepare BAP */
  result = hfa384x_prepare_bap ( hw, id, offset );
  if ( result == 0 ) {
    /* Write even(len) buf contents to data reg */
    for ( i = 0; i < (len & 0xfffe); i+=2 ) {
      hfa384x_setreg_noswap(hw, *(UINT16*)(&(d[i])), HFA384x_DATA0);
    }
    /* If len odd, handle last byte */
    if ( len % 2 ){
      savereg = hfa384x_getreg_noswap(hw, HFA384x_DATA0);
      result = hfa384x_prepare_bap ( hw, id, offset + (len & 0xfffe) );
      if ( result == 0 ) {
	((UINT8*)(&savereg))[0] = d[len-1];
	hfa384x_setreg_noswap(hw, savereg, HFA384x_DATA0);
      }
    }
  }
  if (result) {
    printf ( "copy_to_bap(%#hx, %#hx, %d) failed, result=%#hx\n", id, offset, len, result);
  }
  return result;
}

/*
 * Request a given record to be copied to/from the record buffer.
 *
 * Arguments:
 *	hw		device structure
 *	write		[0|1] copy the record buffer to the given
 *			configuration record. (host order)
 *	rid		RID of the record to read/write. (host order)
 *
 * Returns: 
 *	0		success
 */
static inline int hfa384x_cmd_access(hfa384x_t *hw, UINT16 write, UINT16 rid)
{
  return hfa384x_docmd_wait(hw, HFA384x_CMD_CMDCODE_SET(HFA384x_CMDCODE_ACCESS) | HFA384x_CMD_WRITE_SET(write), rid, 0, 0);
}

/*
 * Performs the sequence necessary to read a config/info item.
 *
 * Arguments:
 *	hw		device structure
 *	rid		config/info record id (host order)
 *	buf		host side record buffer.  Upon return it will
 *			contain the body portion of the record (minus the 
 *			RID and len).
 *	len		buffer length (in bytes, should match record length)
 *
 * Returns: 
 *	0		success
 */
static int hfa384x_drvr_getconfig(hfa384x_t *hw, UINT16 rid, void *buf, UINT16 len)
{
  int result = 0;
  hfa384x_rec_t	rec;

  /* Request read of RID */
  result = hfa384x_cmd_access( hw, 0, rid);
  if ( result ) {
    printf("Call to hfa384x_cmd_access failed\n");
    return -1;
  }
  /* Copy out record length */
  result = hfa384x_copy_from_bap( hw, rid, 0, &rec, sizeof(rec));
  if ( result ) {
    return -1;
  }
  /* Validate the record length */
  if ( ((hfa384x2host_16(rec.reclen)-1)*2) != len ) {  /* note body len calculation in bytes */
    printf ( "RID len mismatch, rid=%#hx hlen=%d fwlen=%d\n", rid, len, (hfa384x2host_16(rec.reclen)-1)*2);
    return -1;
  }
  /* Copy out record data */
  result = hfa384x_copy_from_bap( hw, rid, sizeof(rec), buf, len);
  return result;
}

/*
 * Performs the sequence necessary to read a 16/32 bit config/info item
 * and convert it to host order.
 *
 * Arguments:
 *	hw		device structure
 *	rid		config/info record id (in host order)
 *	val		ptr to 16/32 bit buffer to receive value (in host order)
 *
 * Returns: 
 *	0		success
 */
static int hfa384x_drvr_getconfig16(hfa384x_t *hw, UINT16 rid, void *val)
{
  int result = 0;
  result = hfa384x_drvr_getconfig(hw, rid, val, sizeof(UINT16));
  if ( result == 0 ) {
    *((UINT16*)val) = hfa384x2host_16(*((UINT16*)val));
  }
  return result;
}
static int hfa384x_drvr_getconfig32(hfa384x_t *hw, UINT16 rid, void *val)
{
  int result = 0;
  result = hfa384x_drvr_getconfig(hw, rid, val, sizeof(UINT32));
  if ( result == 0 ) {
    *((UINT32*)val) = hfa384x2host_32(*((UINT32*)val));
  }
  return result;
}

/*
 * Performs the sequence necessary to write a config/info item.
 *
 * Arguments:
 *	hw		device structure
 *	rid		config/info record id (in host order)
 *	buf		host side record buffer
 *	len		buffer length (in bytes)
 *
 * Returns: 
 *	0		success
 */
static int hfa384x_drvr_setconfig(hfa384x_t *hw, UINT16 rid, void *buf, UINT16 len)
{
  int result = 0;
  hfa384x_rec_t	rec;

  rec.rid = host2hfa384x_16(rid);
  rec.reclen = host2hfa384x_16((len/2) + 1); /* note conversion to words, +1 for rid field */
  /* write the record header */
  result = hfa384x_copy_to_bap( hw, rid, 0, &rec, sizeof(rec));
  if ( result ) {
    printf("Failure writing record header\n");
    return -1;
  }
  /* write the record data (if there is any) */
  if ( len > 0 ) {
    result = hfa384x_copy_to_bap( hw, rid, sizeof(rec), buf, len);
    if ( result ) {
      printf("Failure writing record data\n");
      return -1;
    }
  }
  /* Trigger setting of record */
  result = hfa384x_cmd_access( hw, 1, rid);
  return result;
}

/*
 * Performs the sequence necessary to write a 16/32 bit config/info item.
 *
 * Arguments:
 *	hw		device structure
 *	rid		config/info record id (in host order)
 *	val		16/32 bit value to store (in host order)
 *
 * Returns: 
 *	0		success
 */
static int hfa384x_drvr_setconfig16(hfa384x_t *hw, UINT16 rid, UINT16 *val)
{
  UINT16 value;
  value = host2hfa384x_16(*val);
  return hfa384x_drvr_setconfig(hw, rid, &value, sizeof(UINT16));
}
static int hfa384x_drvr_setconfig32(hfa384x_t *hw, UINT16 rid, UINT32 *val)
{
  UINT32 value;
  value = host2hfa384x_32(*val);
  return hfa384x_drvr_setconfig(hw, rid, &value, sizeof(UINT32));
}

/*
 * Wait for an event, with specified checking interval and timeout.
 * Automatically acknolwedges events.
 *
 * Arguments:
 *	hw		device structure
 *      event_mask      EVSTAT register mask of events to wait for
 *      wait            Time (in us) to wait between each poll of the register
 *      timeout         Maximum number of polls before timing out
 *      descr           Descriptive text string of what is being waited for
 *                      (will be printed out if a timeout happens)
 *
 * Returns: 
 *      value of EVSTAT register, or 0 on failure 
 */
static int hfa384x_wait_for_event(hfa384x_t *hw, UINT16 event_mask, int wait, int timeout, const char *descr)
{
  UINT16 reg;
  int count = 0;
  
  do {
    reg = hfa384x_getreg(hw, HFA384x_EVSTAT);
    if ( count > 0 ) udelay(wait);
    count++;
  } while ( !(reg & event_mask) && count < timeout);
  if ( count >= timeout ) {
    printf("hfa384x: Timed out waiting for %s\n", descr);
    return 0; /* Return failure */
  }
  /* Acknowledge all events that we were waiting on */
  hfa384x_setreg(hw, reg & event_mask, HFA384x_EVACK);
  return reg;
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int prism2_poll(struct nic *nic)
{
  UINT16 reg;
  UINT16 rxfid;
  UINT16 result;
  hfa384x_rx_frame_t rxdesc;
  hfa384x_t *hw = &hw_global;
  
  /* Check for received packet */
  reg = hfa384x_getreg(hw, HFA384x_EVSTAT);
  if ( ! HFA384x_EVSTAT_ISRX(reg) ) {
    /* No packet received - return 0 */
    return 0;
  }
  /* Acknowledge RX event */
  hfa384x_setreg(hw, HFA384x_EVACK_RX_SET(1), HFA384x_EVACK);
  /* Get RX FID */  
  rxfid = hfa384x_getreg(hw, HFA384x_RXFID);
  /* Get the descriptor (including headers) */
  result = hfa384x_copy_from_bap(hw, rxfid, 0, &rxdesc, sizeof(rxdesc));
  if ( result ) {
    return 0; /* fail */
  }
  /* Byte order convert once up front. */
  rxdesc.status = hfa384x2host_16(rxdesc.status);
  rxdesc.time = hfa384x2host_32(rxdesc.time);
  rxdesc.data_len = hfa384x2host_16(rxdesc.data_len);

  /* Fill in nic->packetlen */
  nic->packetlen = rxdesc.data_len;
  if ( nic->packetlen > 0 ) {
    /* Fill in nic->packet */
    /*
     * NOTE: Packets as received have an 8-byte header (LLC+SNAP(?)) terminating with the packet type.
     * Etherboot expects a 14-byte header terminating with the packet type (it ignores the rest of the
     * header), so we use a quick hack to achieve this.
     */
    result = hfa384x_copy_from_bap(hw, rxfid, HFA384x_RX_DATA_OFF,
				   nic->packet + ETH_HLEN - sizeof(wlan_80211hdr_t), nic->packetlen);
    if ( result ) {
      return 0; /* fail */
    }
  }
  return 1; /* Packet successfully received */
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void prism2_transmit(
			    struct nic *nic,
			    const char *d,			/* Destination */
			    unsigned int t,			/* Type */
			    unsigned int s,			/* size */
			    const char *p)			/* Packet */
{
  hfa384x_t *hw = &hw_global;
  hfa384x_tx_frame_t txdesc;
  wlan_80211hdr_t p80211hdr = { wlan_llc_snap, {{0,0,0},0} };
  UINT16 fid;
  UINT16 status;
  int result;

  // Request FID allocation
  result = hfa384x_docmd_wait(hw, HFA384x_CMD_CMDCODE_SET(HFA384x_CMDCODE_ALLOC), HFA384x_DRVR_TXBUF_MAX, 0, 0);
  if (result != 0) {
    printf("hfa384x: Tx FID allocate command failed: Aborting transmit..\n");
    return;
  }
  if ( !hfa384x_wait_for_event(hw, HFA384x_EVSTAT_ALLOC, 10, 50, "Tx FID to be allocated\n" ) ) return;
  fid = hfa384x_getreg(hw, HFA384x_ALLOCFID);

  /* Build Tx frame structure */
  memset(&txdesc, 0, sizeof(txdesc));
  txdesc.tx_control = host2hfa384x_16( HFA384x_TX_MACPORT_SET(0) | HFA384x_TX_STRUCTYPE_SET(1) | 
				       HFA384x_TX_TXEX_SET(1) | HFA384x_TX_TXOK_SET(1) );
  txdesc.frame_control =  host2ieee16( WLAN_SET_FC_FTYPE(WLAN_FTYPE_DATA) |
				       WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DATAONLY) |
				       WLAN_SET_FC_TODS(1) );
  memcpy(txdesc.address1, hw->bssid, WLAN_ADDR_LEN);
  memcpy(txdesc.address2, nic->node_addr, WLAN_ADDR_LEN);
  memcpy(txdesc.address3, d, WLAN_ADDR_LEN);
  txdesc.data_len = host2hfa384x_16( sizeof(txdesc) + sizeof(p80211hdr) + s );
  /* Set up SNAP header */
  /* Let OUI default to RFC1042 (0x000000) */
  p80211hdr.snap.type = htons(t);
  
  /* Copy txdesc, p80211hdr and payload parts to FID */
  result = hfa384x_copy_to_bap(hw, fid, 0, &txdesc, sizeof(txdesc));
  if ( result ) return; /* fail */
  result = hfa384x_copy_to_bap( hw, fid, sizeof(txdesc), &p80211hdr, sizeof(p80211hdr) );
  if ( result ) return; /* fail */
  result = hfa384x_copy_to_bap( hw, fid, sizeof(txdesc) + sizeof(p80211hdr), (UINT8*)p, s );
  if ( result ) return; /* fail */

  /* Issue Tx command */
  result = hfa384x_docmd_wait(hw, HFA384x_CMD_CMDCODE_SET(HFA384x_CMDCODE_TX), fid, 0, 0);
  if ( result != 0 ) {
    printf("hfa384x: Transmit failed with result %#hx.\n", result);
    return;
  }
  
  /* Wait for transmit completion (or exception) */
  result = hfa384x_wait_for_event(hw, HFA384x_EVSTAT_TXEXC | HFA384x_EVSTAT_TX,
				  200, 500, "Tx to complete\n" );
  if ( !result ) return; /* timeout failure */
  if ( HFA384x_EVSTAT_ISTXEXC(result) ) {
    fid = hfa384x_getreg(hw, HFA384x_TXCOMPLFID);
    printf ( "Tx exception occurred with fid %#hx\n", fid );
    result = hfa384x_copy_from_bap(hw, fid, 0, &status, sizeof(status));
    if ( result ) return; /* fail */
    printf("hfa384x: Tx error occurred (status %#hx):\n", status);
    if ( HFA384x_TXSTATUS_ISACKERR(status) ) { printf(" ...acknowledgement error\n"); }
    if ( HFA384x_TXSTATUS_ISFORMERR(status) ) { printf(" ...format error\n"); }
    if ( HFA384x_TXSTATUS_ISDISCON(status) ) { printf(" ...disconnected error\n"); }
    if ( HFA384x_TXSTATUS_ISAGEDERR(status) ) { printf(" ...AGED error\n"); }
    if ( HFA384x_TXSTATUS_ISRETRYERR(status) ) { printf(" ...retry error\n"); }
    return; /* fail */
  }
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void prism2_disable(struct dev *dev)
{
  /* put the card in its initial state */

}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
You should omit the last argument struct pci_device * for a non-PCI NIC
***************************************************************************/
#if (WLAN_HOSTIF == WLAN_PLX)
static int prism2_plx_probe(struct dev *dev, struct pci_device *p)
#elif (WLAN_HOSTIF == WLAN_PCI)
static int prism2_pci_probe(struct dev *dev, struct pci_device *p)
#endif
{
  struct nic *nic = (struct nic *)dev;
  int found = 0;
  hfa384x_t *hw = &hw_global;
  int result;
  UINT16 reg;
  UINT16 tmp16 = 0;
  UINT16 infofid;
  hfa384x_InfFrame_t inf;
  char ssid[HFA384x_RID_CNFDESIREDSSID_LEN];

  /* Find and intialise PLX Prism2 card */
#if (WLAN_HOSTIF == WLAN_PLX)
  if ( ! prism2_find_plx ( hw, p ) ) return 0;
#elif (WLAN_HOSTIF == WLAN_PCI)
  if ( ! prism2_find_pci ( hw, p ) ) return 0;
#endif

  /* Initialize card */
  result = hfa384x_docmd_wait(hw, HFA384x_CMDCODE_INIT, 0,0,0); /* Send initialize command */
  if ( result ) printf ( "Initialize command returned %#hx\n", result );
  hfa384x_setreg(hw, 0, HFA384x_INTEN); /* Disable interrupts */
  hfa384x_setreg(hw, 0xffff, HFA384x_EVACK); /* Acknowledge any spurious events */

  /* Retrieve MAC address (and fill out nic->node_addr) */
  hfa384x_drvr_getconfig ( hw, HFA384x_RID_CNFOWNMACADDR, nic->node_addr, HFA384x_RID_CNFOWNMACADDR_LEN );
  printf ( "MAC address %!\n", nic->node_addr );

  /* Prepare card for autojoin */
  /* This procedure is reverse-engineered from a register-level trace of the Linux driver's join process */
  tmp16 = WLAN_DATA_MAXLEN; /* Set maximum data length */
  result = hfa384x_drvr_setconfig16(hw, HFA384x_RID_CNFMAXDATALEN, &tmp16);
  if ( result ) printf ( "Set Max Data Length command returned %#hx\n", result );
  tmp16 = 0x000f; /* Set transmit rate(?) */
  result = hfa384x_drvr_setconfig16(hw, HFA384x_RID_TXRATECNTL, &tmp16);
  if ( result ) printf ( "Set Transmit Rate command returned %#hx\n", result );
  tmp16 = HFA384x_CNFAUTHENTICATION_OPENSYSTEM; /* Set authentication type to OpenSystem */
  result = hfa384x_drvr_setconfig16(hw, HFA384x_RID_CNFAUTHENTICATION, &tmp16);
  if ( result ) printf ( "Set Authentication Type command returned %#hx\n", result );
  /* Set SSID */
  memset(ssid, 0, HFA384x_RID_CNFDESIREDSSID_LEN);
  for ( tmp16=0; tmp16<sizeof(hardcoded_ssid); tmp16++ ) { ssid[2+tmp16] = hardcoded_ssid[tmp16]; }
  ssid[0] = sizeof(hardcoded_ssid) - 1; /* Ignore terminating zero */
  result = hfa384x_drvr_setconfig(hw, HFA384x_RID_CNFDESIREDSSID, ssid, HFA384x_RID_CNFDESIREDSSID_LEN); /* Set the SSID */
  if ( result ) printf ( "Set SSID command returned %#hx\n", result );
  tmp16 = 1; /* Set port type to ESS port */
  result = hfa384x_drvr_setconfig16(hw, HFA384x_RID_CNFPORTTYPE, &tmp16);
  if ( result ) printf ( "Set port type command returned %#hx\n", result );
  /* Enable card */
  result = hfa384x_docmd_wait(hw, HFA384x_CMD_CMDCODE_SET(HFA384x_CMDCODE_ENABLE) | HFA384x_CMD_MACPORT_SET(0), 0,0,0);
  if ( result ) printf ( "Enable command returned %#hx\n", result );

  /* Wait for info frame to indicate link status */
  if ( sizeof(hardcoded_ssid) == 1 ) {
    /* Empty SSID => join to any SSID */
    printf("Attempting to autojoin to any available access point...");
  } else {
    printf("Attempting to autojoin to SSID %s...", &ssid[2]);
  }
  if ( !hfa384x_wait_for_event(hw, HFA384x_EVSTAT_INFO, 1000, 2000, "Info event" ) ) return 0;
  printf("done\n");
  infofid = hfa384x_getreg(hw, HFA384x_INFOFID);
  /* Retrieve the length */
  result = hfa384x_copy_from_bap( hw, infofid, 0, &inf.framelen, sizeof(UINT16));
  if ( result ) return 0; /* fail */
  inf.framelen = hfa384x2host_16(inf.framelen);
  /* Retrieve the rest */
  result = hfa384x_copy_from_bap( hw, infofid, sizeof(UINT16),
				  &(inf.infotype), inf.framelen * sizeof(UINT16));
  if ( result ) return 0; /* fail */
  if ( inf.infotype != HFA384x_IT_LINKSTATUS ) {
    /* Not a Link Status info frame: die */
    printf ( "Unexpected info frame type %#hx (not LinkStatus type)\n", inf.infotype );
    return 0;
  }
  inf.info.linkstatus.linkstatus = hfa384x2host_16(inf.info.linkstatus.linkstatus);
  if ( inf.info.linkstatus.linkstatus != HFA384x_LINK_CONNECTED ) {
    /* Link not connected - die */
    printf ( "Link not connected (status %#hx)\n", inf.info.linkstatus.linkstatus );
    return 0;
  }

  /* Retrieve BSSID and print Connected message */
  result = hfa384x_drvr_getconfig(hw, HFA384x_RID_CURRENTBSSID, hw->bssid, WLAN_BSSID_LEN);
  printf ( "Link connected (BSSID %! - MAC address %!)\n", hw->bssid, nic->node_addr );
  
  /* point to NIC specific routines */
  dev->disable  = prism2_disable; 
  nic->poll     = prism2_poll;
  nic->transmit = prism2_transmit;
  return 1;
}

/*
 * Find PLX card.  Prints out information strings from PCMCIA CIS as visual
 * confirmation of presence of card.
 *
 * Arguments:
 *	hw		device structure to be filled in
 *      p               PCI device structure
 *
 * Returns:
 *      1               Success
 */
static int prism2_find_plx ( hfa384x_t *hw, struct pci_device *p )
{
  int found = 0;
  uint32_t plx_lcr  = 0; /* PLX9052 Local Configuration Register Base (I/O) */
  uint32_t attr_mem = 0; /* Prism2 Attribute Memory Base */
  uint32_t iobase   = 0; /* Prism2 I/O Base */
  unsigned char *cis_tpl  = NULL;
  unsigned char *cis_string;
  
  /* Obtain all memory and IO base addresses */
  pcibios_read_config_dword( p->bus, p->devfn, PLX_LOCAL_CONFIG_REGISTER_BASE, &plx_lcr);
  plx_lcr &= PCI_BASE_ADDRESS_IO_MASK;
  pcibios_read_config_dword( p->bus, p->devfn, PRISM2_PLX_ATTR_MEM_BASE, &attr_mem);
  pcibios_read_config_dword( p->bus, p->devfn, PRISM2_PLX_IO_BASE, &iobase);
  iobase &= PCI_BASE_ADDRESS_IO_MASK;

  /* Fill out hw structure */
  hw->membase = attr_mem;
  hw->iobase = iobase;
  printf ( "PLX9052 has local config registers at %#hx\n", plx_lcr );
  printf ( "Prism2 has attribute memory at %#x and I/O base at %#hx\n", attr_mem, iobase );

  /* Search for CIS strings */
  printf ( "Searching for PCMCIA card...\n" );
  cis_tpl = bus_to_virt(attr_mem);
  while ( *cis_tpl != CISTPL_END ) {
    if ( *cis_tpl == CISTPL_VERS_1 ) {
      /* CISTPL_VERS_1 contains some nice text strings */
      printf ( "...found " );
      found = 1;
      cis_string = cis_tpl + CISTPL_VERS_1_STR_OFF;
      while ( ! ( ( *cis_string == 0 ) && ( *(cis_string+CIS_STEP) == 0 ) ) ) {
	printf ( "%c", *cis_string == 0 ? ' ' : *cis_string );
	cis_string += CIS_STEP;
      }
      printf ( "\n" );
    }
    /* printf ( "CIS tuple type %#hhx, length %#hhx\n", *cis_tpl, *(cis_tpl+CISTPL_LEN_OFF) ); */
    cis_tpl += CISTPL_HEADER_LEN + CIS_STEP * ( *(cis_tpl+CISTPL_LEN_OFF) );
  }
  if ( found == 0 ) {
    printf ( "...nothing found\n" );
  }
  ((unsigned char *)bus_to_virt(attr_mem))[COR_OFFSET] = COR_VALUE; /* Write COR to enable PC card */
  return found;
}

/*
 * Find PCI card.
 *
 * Arguments:
 *	hw		device structure to be filled in
 *      p               PCI device structure
 *
 * Returns:
 *      1               Success
 */
static int prism2_find_pci ( hfa384x_t *hw, struct pci_device *p )
{
  uint32_t membase = 0; /* Prism2.5 Memory Base */
  pcibios_read_config_dword( p->bus, p->devfn, PRISM2_PCI_MEM_BASE, &membase);
  membase &= PCI_BASE_ADDRESS_MEM_MASK;
  hw->membase = membase;
  printf ( "Prism2.5 has registers at %#x\n", membase );
  return 1;
}

#if (WLAN_HOSTIF == WLAN_PLX)
static struct pci_id prism2_plx_nics[] = {
	{ PCI_VENDOR_ID_NETGEAR,	PCI_DEVICE_ID_NETGEAR_MA301,
	  "Netgear MA301" },
};

static struct pci_driver prism2_plx_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "Prism2_PLX",
	.probe    = prism2_plx_probe,
	.ids      = prism2_plx_nics,
	.id_count = sizeof(prism2_plx_nics)/sizeof(prism2_plx_nics[0]),
};
#endif /* WLAN_PLX */


#if (WLAN_HOSTIF == WLAN_PCI)
static struct pci_id prism2_pci_nics[] = {
	{ PCI_VENDOR_ID_HARRIS,		PCI_DEVICE_ID_HARRIS_PRISM2,
	  "Harris Semiconductor Prism2.5 clone" },
};

static struct pci_driver prism2_pci_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "Prism2_PCI",
	.probe    = prism2_pci_probe,
	.ids      = prism2_pci_nics,
	.id_count = sizeof(prism2_pci_nics)/sizeof(prism2_pci_nics[0]),
};
#endif /* WLAN_PCI */
