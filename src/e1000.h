/*****************************************************************************
 *****************************************************************************

 Copyright (c) 1999-2001 Intel Corporation. 

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

/*************************************************************************/
/*! \file   e1000.h
 *  \author Intel Corporation
 *  \date   1999-2001
 *  \brief  Linux PRO/1000 Ethernet Driver main header file
 *************************************************************************/

#ifndef _E1000_H_
#define _E1000_H_

struct _ADAPTER_STRUCT;
typedef struct _ADAPTER_STRUCT ADAPTER_STRUCT, *PADAPTER_STRUCT;


/* Prevent name space polution */

#define IN
#define OUT
#define STATIC static

#include "e1000_fxhw.h"
#include "e1000_phy.h"


#define PCI_DEVICE_ID_82542        0x1000
#define PCI_DEVICE_ID_82543GC_FIBER  0x1001
#define PCI_DEVICE_ID_82543GC_COPPER 0x1004
#define PCI_DEVICE_ID_82544EI_COPPER 0x1008
#define PCI_DEVICE_ID_82544GC_CREB   0x100D
#define BAR_0 0

/* 8254x can use Dual Address Cycles for 64-bit addressing */
/* Advertise that we can DMA from any address location */
#define E1000_DMA_MASK (~0x0UL)
#define E1000_DBG(args...)
// #define E1000_DBG(args...) printk("e1000: " args)
#define E1000_ERR(args...) printf("e1000: " args)
#define E1000_MAX_INTR 10
#define MAX_NUM_MULTICAST_ADDRESSES 128

#define B_TRUE  1
#define B_FALSE 0
#define TRUE    1
#define FALSE   0

/* command line options defaults */
#define DEFAULT_TXD     256
#define MAX_TXD         256
#define MIN_TXD          80
#define DEFAULT_RXD     256
#define MAX_RXD         256
#define MIN_RXD          80
#define DEFAULT_TIDV     64
#define MAX_TIDV     0xFFFF
#define MIN_TIDV          0
#define DEFAULT_RIDV     64
#define MAX_RIDV     0xFFFF
#define MIN_RIDV          0

#define OPTION_UNSET    -1
#define OPTION_DISABLED 0
#define OPTION_ENABLED  1
#define JUMBO_DEFAULT        OPTION_ENABLED
#define XSUMRX_DEFAULT       OPTION_ENABLED
#define WAITFORLINK_DEFAULT  OPTION_ENABLED
#define AUTONEG_ADV_DEFAULT  0x2F
#define AUTONEG_ADV_MASK     0x2F
#define FLOW_CONTROL_DEFAULT FLOW_CONTROL_FULL

#define E1000_REPORT_TX_EARLY  2
#define E1000_PCI_MWI_ENABLE   1
#define E1000_PCI_MLT_OVERRIDE 0

#define E1000_BUS_TYPE_PCI               1
#define E1000_BUS_TYPE_PCIX              2

#define E1000_BUS_SPEED_PCI_33MHZ        1
#define E1000_BUS_SPEED_PCI_66MHZ        2
#define E1000_BUS_SPEED_PCIX_50_66MHZ    3
#define E1000_BUS_SPEED_PCIX_66_100MHZ   4
#define E1000_BUS_SPEED_PCIX_100_133MHZ  5
#define E1000_BUS_SPEED_PCIX_RESERVED    6

#define E1000_BUS_WIDTH_32_BIT           1
#define E1000_BUS_WIDTH_64_BIT           2

/* Supported RX Buffer Sizes */
#define E1000_RXBUFFER_2048        2048
#define E1000_RXBUFFER_4096        4096
#define E1000_RXBUFFER_8192        8192
#define E1000_RXBUFFER_16384      16384

#define ETH_LENGTH_OF_ADDRESS 6

#define RET_STATUS_SUCCESS 0
#define RET_STATUS_FAILURE 1

#define E1000_PCI

/* The size in bytes of a standard ethernet header */
#define ENET_HEADER_SIZE    14
#define MAX_INTS            256
#define CRC_LENGTH 4

#define MAXIMUM_ETHERNET_PACKET_SIZE    1514
#define RCV_PKT_MUL              5

/* Round size up to the next multiple of unit */
#define E1000_ROUNDUP(size, unit) ((((size) + (unit) - 1) / (unit)) * (unit))
/* This is better, but only works for unit sizes that are powers of 2 */
#define E1000_ROUNDUP2(size, unit) (((size) + (unit) - 1) & ~((unit) - 1))

/* wrapper around a pointer to a socket buffer,
 * so a DMA handle can be stored along with the buffer */
#if 0
struct e1000_buffer {
    struct sk_buff *skb;
    uint64_t       dma;
    unsigned long  length;
};
#endif

/* Adapter->flags definitions */
#define E1000_BOARD_OPEN 0

typedef enum _XSUM_CONTEXT_T {
    OFFLOAD_NONE,
    OFFLOAD_TCP_IP,
    OFFLOAD_UDP_IP
} XSUM_CONTEXT_T;

/* board specific private data structure */

struct _ADAPTER_STRUCT {
    struct _ADAPTER_STRUCT *next;
    struct _ADAPTER_STRUCT *prev;

#ifdef IANS
    void *iANSReserved;
    piANSsupport_t iANSdata;
    UINT32 ans_link;
    UINT32 ans_speed;
    UINT32 ans_duplex;
    UINT32 ans_suspend;
    IANS_BD_TAGGING_MODE tag_mode;
#endif

    /* MMIO PCI Registers */
    volatile PE1000_REGISTERS HardwareVirtualAddress;


    uint32_t flags;
    uint32_t bd_number;

    /* Ethernet Node Address */
    uint8_t CurrentNetAddress[ETH_LENGTH_OF_ADDRESS];
    uint8_t PermNetAddress[ETH_LENGTH_OF_ADDRESS];

    /* Mac and Phy Settings */
    uint8_t MacType;
    uint8_t MediaType;
    uint32_t PhyId;
    uint32_t PhyAddress;

    /* MAC Configuration */
    uint8_t AutoNeg;
    uint8_t ForcedSpeedDuplex;
    uint16_t AutoNegAdvertised;
    uint8_t WaitAutoNegComplete;

    /* Status Flags */
    BOOLEAN LinkIsActive;
    uint16_t LineSpeed;
    uint16_t FullDuplex;
    BOOLEAN GetLinkStatus;
    BOOLEAN LinkStatusChanged;
    BOOLEAN LongPacket;
    uint32_t RxBufferLen;
    BOOLEAN TbiCompatibilityEnable;
    BOOLEAN TbiCompatibilityOn;

    /* PCI Device Info */
    uint16_t VendorId;
    uint16_t DeviceId;
    uint8_t RevId;
    uint16_t SubVendorId;
    uint16_t SubSystemId;

    /* PCI Bus Info */
    uint8_t BusType;
    uint8_t BusSpeed;
    uint8_t BusWidth;

    UINT32 PartNumber;

    BOOLEAN NosEnabledMulticastPromiscuous;

    BOOLEAN AdapterStopped;
    uint32_t IntMask;
    uint16_t PciCommandWord;
    uint8_t DmaFairness;
    uint32_t OriginalFlowControl;
    uint8_t FlowControl;
    uint16_t FlowControlHighWatermark;
    uint16_t FlowControlLowWatermark;
    uint16_t FlowControlPauseTime;
    BOOLEAN FlowControlSendXon;
    BOOLEAN ReportTxEarly;
    uint32_t TxcwRegValue;
    uint32_t AutoNegFailed;

    UINT32 MaxFrameSize;
#if 0
    /* e1000_adaptive */
    BOOLEAN AdaptiveTxThreshold;
    UINT32 CurrentTxThreshold;
    UINT8 TxThresholdTimer;
    BOOLEAN AdaptiveIFS;
    BOOLEAN InIFSMode;
    BOOLEAN IFSParamsForced;
    UINT16 CurrentIFSValue;
    UINT16 IFSMaxValue;
    UINT16 IFSMinValue;
    UINT16 IFSStepSize;
    UINT16 IFSRatio;
#endif
    struct pci_device *pdev;
    /* driver specific */

    int NumTxDescriptors;
    uint32_t TxIntDelay;
    int NextAvailTxDescriptor;
    int OldestUsedTxDescriptor;
    E1000_TRANSMIT_DESCRIPTOR *TxDescriptors;
    dma_addr_t TxDescDMA;
    struct e1000_buffer *tx_skb;
    u32 NumTxDescriptorsAvail;
    uint32_t TxdCmd;

    int NumRxDescriptors;
    uint32_t RxIntDelay;
    int NextRxDescriptorToCheck;
    E1000_RECEIVE_DESCRIPTOR *RxDescriptors;
    dma_addr_t RxDescDMA;
    struct e1000_buffer *rx_skb;
    atomic_t NumRxDescriptorsEmpty;
    int NextRxDescriptorToFill;
    atomic_t tx_timeout;

    uint64_t XsumRXGood;
    uint64_t XsumRXError;
    
    /* Linux driver specific */
    char *id_string;
    uint8_t JumboFrames;
    uint8_t RxChecksum;
    XSUM_CONTEXT_T ActiveChecksumContext;

    /* Multicast Stuff */
    UINT32 MulticastFilterType;
    UINT32 NumberOfMcAddresses;
    UCHAR MulticastAddressList[MAX_NUM_MULTICAST_ADDRESSES][ETH_LENGTH_OF_ADDRESS];
    /* calling these out as unsigned longs helps match up with the OS
     * stats, which as unsigned longs change from 32 to 64 bits when moving
     * to a 64-bit CPU, that change should not effect us */

    /* statistic registers present in the 82542 */
    unsigned long Crcerrs;
    unsigned long Symerrs;
    unsigned long Mpc;
    unsigned long Scc;
    unsigned long Ecol;
    unsigned long Mcc;
    unsigned long Latecol;
    unsigned long Colc;
    unsigned long Dc;
    unsigned long Sec;
    unsigned long Rlec;
    unsigned long Xonrxc;
    unsigned long Xontxc;
    unsigned long Xoffrxc;
    unsigned long Xofftxc;
    unsigned long Fcruc;
    unsigned long Prc64;
    unsigned long Prc127;
    unsigned long Prc255;
    unsigned long Prc511;
    unsigned long Prc1023;
    unsigned long Prc1522;
    unsigned long Gprc;
    unsigned long Bprc;
    unsigned long Mprc;
    unsigned long Gptc;
    unsigned long Gorcl;
    unsigned long Gorch;
    unsigned long Gotcl;
    unsigned long Gotch;
    unsigned long Rnbc;
    unsigned long Ruc;
    unsigned long Rfc;
    unsigned long Roc;
    unsigned long Rjc;
    unsigned long Torcl;
    unsigned long Torch;
    unsigned long Totcl;
    unsigned long Totch;
    unsigned long Tpr;
    unsigned long Tpt;
    unsigned long Ptc64;
    unsigned long Ptc127;
    unsigned long Ptc255;
    unsigned long Ptc511;
    unsigned long Ptc1023;
    unsigned long Ptc1522;
    unsigned long Mptc;
    unsigned long Bptc;
    /* statistic registers added in the 82543 */
    unsigned long Algnerrc;
    unsigned long Rxerrc;
    unsigned long Tuc;
    unsigned long Tncrs;
    unsigned long Cexterr;
    unsigned long Rutec;
    unsigned long Tsctc;
    unsigned long Tsctfc;
};

/* PCI defines needed by e1000_fxhw.c */
/* define these using the Linux names for the same thing (pci.h) */
#define CMD_MEM_WRT_INVALIDATE PCI_COMMAND_INVALIDATE
#define PCI_COMMAND_REGISTER PCI_COMMAND

/* Prototypes */
/* e1000_main.c */
/* e1000_fxhw.c */
extern VOID AdapterStop(void);
extern BOOLEAN InitializeHardware(void);
extern VOID InitRxAddresses(void);
extern BOOLEAN SetupFlowControlAndLink(void);
extern BOOLEAN SetupPcsLink(UINT32 DeviceControlReg);
extern VOID ConfigFlowControlAfterLinkUp(void);
extern VOID ForceMacFlowControlSetting(void);
extern VOID CheckForLink(void);
extern VOID ClearHwStatsCounters(void);
extern VOID GetSpeedAndDuplex(PUINT16 Speed, PUINT16 Duplex);
extern USHORT ReadEepromWord(USHORT Reg);
extern BOOLEAN ValidateEepromChecksum(void);
extern VOID UpdateEepromChecksum(void);
extern BOOLEAN WriteEepromWord(USHORT Reg, USHORT Data);
extern UINT32 ReadPartNumber(void);
extern VOID IdLedOn(void);
extern VOID IdLedOff(void);
extern VOID MulticastAddressListUpdate(PUCHAR MulticastAddressList,
                               UINT32 MulticastAddressCount, UINT32 Padding);
extern VOID AdjustTbiAcceptedStats(UINT32 FrameLength, PUINT8 MacAddress);
extern VOID WriteVfta(UINT32 Offset, UINT32 Value);
extern VOID ClearVfta(void);

/* e1000_phy.h */
extern UINT16 ReadPhyRegister(UINT32 RegAddress, UINT32 PhyAddress);
extern VOID PhyHardwareReset(void);
/*  #include "e1000_adaptive.h" */
#endif /* _E1000_H_ */
