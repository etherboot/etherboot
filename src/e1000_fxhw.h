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

/* Workfile: fxhw.h  */
/* Revision: 37  */
/* Date: 7/19/01 5:12p  */

#ifndef _FXHW_
#define _FXHW_
#include "e1000.h"

#define _FXHW_

#define ASSERT(x) if(!(x)) printf("E1000: Asser Fail")
#define DelayInMicroseconds(x) udelay(x)
#define DelayInMilliseconds(x) mdelay(x)

typedef uint8_t UCHAR, UINT8, BOOLEAN, *PUCHAR, *PUINT8;
typedef uint16_t USHORT, UINT16, *PUSHORT, *PUINT16;
typedef uint32_t UINT, ULONG, UINT32, *PUINT, *PULONG, *PUINT32;
typedef void VOID, *PVOID;
typedef struct _E1000_64_BIT_PHYSICAL_ADDRESS {
    UINT32 Lo32;
    UINT32 Hi32;
} E1000_64_BIT_PHYSICAL_ADDRESS, *PE1000_64_BIT_PHYSICAL_ADDRESS;

#define IN
#define OUT
#define STATIC static

#define MSGOUT(S, A, B)     printk(S "\n", A, B)
#ifdef DEBUG
#define DEBUGFUNC(F)        DEBUGOUT(F);
#define DEBUGOUT(S) printf(S)
#define DEBUGOUT1(S,A) printf(S,A)
#define DEBUGOUT2(S,A,B) printf(S,A,B)
#define DEBUGOUT3(S,A,B,C) printf(S,A,B,C)
#define DEBUGOUT7(S,A,B,C,D,E,F,G) printf(S,A,B,C,D,E,F,G)

#else
#define DEBUGFUNC(F)
#define DEBUGOUT(S)
#define DEBUGOUT1(S,A)
#define DEBUGOUT2(S,A,B)
#define DEBUGOUT3(S,A,B,C)
#define DEBUGOUT7(S,A,B,C,D,E,F,G)
#endif

#define WritePciConfigWord(Reg, PValue) pcibios_write_config_word(pci_dev->bus,pci_dev->devfn, Reg, *PValue)

#define MAC_DECODE_SIZE (128 * 1024)
/*
 * We are going to using them for pre process to cut down code size
 * default build for MAC_LIVENGOOD
 *
typedef enum _MAC_TYPE {
    MAC_WISEMAN_2_0 = 0,
    MAC_WISEMAN_2_1,
    MAC_LIVENGOOD,
    MAC_WAINWRIGHT,
    MAC_CORDOVA,
    NUM_MACS
} MAC_TYPE, *PMAC_TYPE;
*/

#define MAC_WISEMAN_2_0 0
#define MAC_WISEMAN_2_1 1
#define MAC_LIVENGOOD 2
#define MAC_WAINWRIGHT 3
#define MAC_CORDOVA 4
#define NUM_MACS 5

#ifndef MACTYPE
#define MACTYPE MAC_LIVENGOOD
#endif

#define WISEMAN_2_0_REV_ID               2
#define WISEMAN_2_1_REV_ID               3

typedef enum _GIGABIT_MEDIA_TYPE {
    MEDIA_TYPE_COPPER = 0,
    MEDIA_TYPE_FIBER = 1,
    NUM_MEDIA_TYPES
} GIGABIT_MEDIA_TYPE, *PGIGABIT_MEDIA_TYPE;

typedef enum _SPEED_DUPLEX_TYPE {
    HALF_10 = 0,
    FULL_10 = 1,
    HALF_100 = 2,
    FULL_100 = 3
} SPEED_DUPLEX_TYPE, *PSPEED_DUPLEX_TYPE;

#define SPEED_10                        10
#define SPEED_100                      100
#define SPEED_1000                    1000
#define HALF_DUPLEX                      1
#define FULL_DUPLEX                      2

#define ENET_HEADER_SIZE                14
#define MAXIMUM_ETHERNET_PACKET_SIZE  1514
#define MINIMUM_ETHERNET_PACKET_SIZE    60
#define CRC_LENGTH                       4

#define MAX_JUMBO_FRAME_SIZE    (0x3F00)

#define ISL_CRC_LENGTH                         4

#define MAXIMUM_VLAN_ETHERNET_PACKET_SIZE   1514
#define MINIMUM_VLAN_ETHERNET_PACKET_SIZE     60
#define VLAN_TAG_SIZE                          4

#define ETHERNET_IEEE_VLAN_TYPE     0x8100
#define ETHERNET_IP_TYPE            0x0800
#define ETHERNET_IPX_TYPE           0x8037
#define ETHERNET_IPX_OLD_TYPE       0x8137
#define MAX_802_3_LEN_FIELD         0x05DC

#define ETHERNET_ARP_TYPE           0x0806
#define ETHERNET_XNS_TYPE           0x0600
#define ETHERNET_X25_TYPE           0x0805
#define ETHERNET_BANYAN_TYPE        0x0BAD
#define ETHERNET_DECNET_TYPE        0x6003
#define ETHERNET_APPLETALK_TYPE     0x809B
#define ETHERNET_SNA_TYPE           0x80D5
#define ETHERNET_SNMP_TYPE          0x814C

#define IP_OFF_MF_BIT               0x0002
#define IP_OFF_OFFSET_MASK          0xFFF8
#define IP_PROTOCOL_ICMP                 1
#define IP_PROTOCOL_IGMP                 2
#define IP_PROTOCOL_TCP                  6
#define IP_PROTOCOL_UDP               0x11
#define IP_PROTOCOL_IPRAW             0xFF

#define POLL_IMS_ENABLE_MASK (E1000_IMS_RXDMT0 | E1000_IMS_RXSEQ)

#define IMS_ENABLE_MASK (E1000_IMS_RXT0 | E1000_IMS_TXDW | E1000_IMS_RXDMT0 | E1000_IMS_RXSEQ | E1000_IMS_LSC)

#define E1000_RAR_ENTRIES 16

typedef struct _E1000_RECEIVE_DESCRIPTOR {
    E1000_64_BIT_PHYSICAL_ADDRESS BufferAddress;

    USHORT Length;
    USHORT Csum;
    UCHAR ReceiveStatus;
    UCHAR Errors;
    USHORT Special;

} E1000_RECEIVE_DESCRIPTOR, *PE1000_RECEIVE_DESCRIPTOR;

#define MIN_NUMBER_OF_DESCRIPTORS (8)
#define MAX_NUMBER_OF_DESCRIPTORS (0xFFF8)

#define E1000_RXD_STAT_DD        (0x01)
#define E1000_RXD_STAT_EOP       (0x02)

#define E1000_RXD_STAT_ISL       (0x04)
#define E1000_RXD_STAT_IXSM      (0x04)
#define E1000_RXD_STAT_VP        (0x08)
#define E1000_RXD_STAT_BPDU      (0x10)
#define E1000_RXD_STAT_TCPCS     (0x20)
#define E1000_RXD_STAT_IPCS      (0x40)

#define E1000_RXD_STAT_PIF       (0x80)

#define E1000_RXD_ERR_CE         (0x01)
#define E1000_RXD_ERR_SE         (0x02)
#define E1000_RXD_ERR_SEQ        (0x04)

#define E1000_RXD_ERR_ICE        (0x08)

#define E1000_RXD_ERR_CXE        (0x10)

#define E1000_RXD_ERR_TCPE       (0x20)
#define E1000_RXD_ERR_IPE        (0x40)

#define E1000_RXD_ERR_RXE        (0x80)

#define E1000_RXD_ERR_FRAME_ERR_MASK (E1000_RXD_ERR_CE | E1000_RXD_ERR_SE | E1000_RXD_ERR_SEQ | E1000_RXD_ERR_CXE | E1000_RXD_ERR_RXE)

#define E1000_RXD_SPC_VLAN_MASK  (0x0FFF)
#define E1000_RXD_SPC_PRI_MASK   (0xE000)
#define E1000_RXD_SPC_PRI_SHIFT  (0x000D)
#define E1000_RXD_SPC_CFI_MASK   (0x1000)
#define E1000_RXD_SPC_CFI_SHIFT  (0x000C)

#define E1000_TXD_DTYP_D        (0x00100000)
#define E1000_TXD_DTYP_C        (0x00000000)
#define E1000_TXD_POPTS_IXSM    (0x01)
#define E1000_TXD_POPTS_TXSM    (0x02)

typedef struct _E1000_TRANSMIT_DESCRIPTOR {
    E1000_64_BIT_PHYSICAL_ADDRESS BufferAddress;

    union {
        ULONG DwordData;
        struct _TXD_FLAGS {
            USHORT Length;
            UCHAR Cso;
            UCHAR Cmd;
        } Flags;
    } Lower;

    union {
        ULONG DwordData;
        struct _TXD_FIELDS {
            UCHAR TransmitStatus;
            UCHAR Css;
            USHORT Special;
        } Fields;
    } Upper;

} E1000_TRANSMIT_DESCRIPTOR, *PE1000_TRANSMIT_DESCRIPTOR;

typedef struct _E1000_TCPIP_CONTEXT_TRANSMIT_DESCRIPTOR {
    union {
        ULONG IpXsumConfig;
        struct _IP_XSUM_FIELDS {
            UCHAR Ipcss;
            UCHAR Ipcso;
            USHORT Ipcse;
        } IpFields;
    } LowerXsumSetup;

    union {
        ULONG TcpXsumConfig;
        struct _TCP_XSUM_FIELDS {
            UCHAR Tucss;
            UCHAR Tucso;
            USHORT Tucse;
        } TcpFields;
    } UpperXsumSetup;

    ULONG CmdAndLength;

    union {
        ULONG DwordData;
        struct _TCP_SEG_FIELDS {
            UCHAR Status;
            UCHAR HdrLen;
            USHORT Mss;
        } Fields;
    } TcpSegSetup;

} E1000_TCPIP_CONTEXT_TRANSMIT_DESCRIPTOR,
    *PE1000_TCPIP_CONTEXT_TRANSMIT_DESCRIPTOR;

typedef struct _E1000_TCPIP_DATA_TRANSMIT_DESCRIPTOR {
    E1000_64_BIT_PHYSICAL_ADDRESS BufferAddress;

    union {
        ULONG DwordData;
        struct _TXD_OD_FLAGS {
            USHORT Length;
            UCHAR TypLenExt;
            UCHAR Cmd;
        } Flags;
    } Lower;

    union {
        ULONG DwordData;
        struct _TXD_OD_FIELDS {
            UCHAR TransmitStatus;
            UCHAR Popts;
            USHORT Special;
        } Fields;
    } Upper;

} E1000_TCPIP_DATA_TRANSMIT_DESCRIPTOR,
    *PE1000_TCPIP_DATA_TRANSMIT_DESCRIPTOR;

#define E1000_TXD_CMD_EOP   (0x01000000)
#define E1000_TXD_CMD_IFCS  (0x02000000)

#define E1000_TXD_CMD_IC    (0x04000000)

#define E1000_TXD_CMD_RS    (0x08000000)
#define E1000_TXD_CMD_RPS   (0x10000000)

#define E1000_TXD_CMD_DEXT  (0x20000000)
#define E1000_TXD_CMD_ISLVE (0x40000000)

#define E1000_TXD_CMD_IDE   (0x80000000)

#define E1000_TXD_STAT_DD   (0x00000001)
#define E1000_TXD_STAT_EC   (0x00000002)
#define E1000_TXD_STAT_LC   (0x00000004)
#define E1000_TXD_STAT_TU   (0x00000008)

#define E1000_TXD_CMD_TCP   (0x01000000)
#define E1000_TXD_CMD_IP    (0x02000000)
#define E1000_TXD_CMD_TSE   (0x04000000)

#define E1000_TXD_STAT_TC   (0x00000004)

#define E1000_NUM_UNICAST          (16)
#define E1000_MC_TBL_SIZE          (128)

#define E1000_VLAN_FILTER_TBL_SIZE (128)

enum {
    FLOW_CONTROL_NONE = 0,
    FLOW_CONTROL_RECEIVE_PAUSE = 1,
    FLOW_CONTROL_TRANSMIT_PAUSE = 2,
    FLOW_CONTROL_FULL = 3,
    FLOW_CONTROL_HW_DEFAULT = 0xFF
};

typedef struct {
    volatile UINT32 Low;
    volatile UINT32 High;
} RECEIVE_ADDRESS_REGISTER_PAIR;

#define E1000_NUM_MTA_REGISTERS 128

typedef struct {
    volatile UINT32 IpAddress;
    volatile UINT32 Reserved;
} IPAT_ENTRY;

#define E1000_WAKEUP_IP_ADDRESS_COUNT_MAX   (4)
#define E1000_IPAT_SIZE                     E1000_WAKEUP_IP_ADDRESS_COUNT_MAX

typedef struct {
    volatile UINT32 Length;
    volatile UINT32 Reserved;
} FFLT_ENTRY;

typedef struct {
    volatile UINT32 Mask;
    volatile UINT32 Reserved;
} FFMT_ENTRY;

typedef struct {
    volatile UINT32 Value;
    volatile UINT32 Reserved;
} FFVT_ENTRY;

#define E1000_FLEXIBLE_FILTER_COUNT_MAX     (4)

#define E1000_FLEXIBLE_FILTER_SIZE_MAX      (128)

#define E1000_FFLT_SIZE                     E1000_FLEXIBLE_FILTER_COUNT_MAX
#define E1000_FFMT_SIZE                     E1000_FLEXIBLE_FILTER_SIZE_MAX
#define E1000_FFVT_SIZE                     E1000_FLEXIBLE_FILTER_SIZE_MAX

typedef struct _E1000_REGISTERS {

    volatile UINT32 Ctrl;
    volatile UINT32 Pad1;
    volatile UINT32 Status;
    volatile UINT32 Pad2;
    volatile UINT32 Eecd;
    volatile UINT32 Pad3;
    volatile UINT32 Exct;
    volatile UINT32 Pad4;
    volatile UINT32 Mdic;
    volatile UINT32 Pad5;
    volatile UINT32 Fcal;
    volatile UINT32 Fcah;
    volatile UINT32 Fct;
    volatile UINT32 Pad6;

    volatile UINT32 Vet;
    volatile UINT32 Pad7;

    RECEIVE_ADDRESS_REGISTER_PAIR Rar[16];

    volatile UINT32 Icr;
    volatile UINT32 Pad8;
    volatile UINT32 Ics;
    volatile UINT32 Pad9;
    volatile UINT32 Ims;
    volatile UINT32 Pad10;
    volatile UINT32 Imc;
    volatile UINT8 Pad11[0x24];

    volatile UINT32 Rctl;
    volatile UINT32 Pad12;
    volatile UINT32 PadRdtr0;
    volatile UINT32 Pad13;
    volatile UINT32 PadRdbal0;
    volatile UINT32 PadRdbah0;
    volatile UINT32 PadRdlen0;
    volatile UINT32 Pad14;
    volatile UINT32 PadRdh0;
    volatile UINT32 Pad15;
    volatile UINT32 PadRdt0;
    volatile UINT32 Pad16;
    volatile UINT32 Rdtr1;
    volatile UINT32 Pad17;
    volatile UINT32 Rdbal1;
    volatile UINT32 Rdbah1;
    volatile UINT32 Rdlen1;
    volatile UINT32 Pad18;
    volatile UINT32 Rdh1;
    volatile UINT32 Pad19;
    volatile UINT32 Rdt1;
    volatile UINT8 Pad20[0x0C];
    volatile UINT32 PadFcrth;
    volatile UINT32 Pad21;
    volatile UINT32 PadFcrtl;
    volatile UINT32 Pad22;
    volatile UINT32 Fcttv;
    volatile UINT32 Pad23;
    volatile UINT32 Txcw;
    volatile UINT32 Pad24;
    volatile UINT32 Rxcw;
    volatile UINT8 Pad25[0x7C];
    volatile UINT32 Mta[(128)];

    volatile UINT32 Tctl;
    volatile UINT32 Pad26;
    volatile UINT32 Tqsal;
    volatile UINT32 Tqsah;
    volatile UINT32 Tipg;
    volatile UINT32 Pad27;
    volatile UINT32 Tqc;
    volatile UINT32 Pad28;
    volatile UINT32 PadTdbal;
    volatile UINT32 PadTdbah;
    volatile UINT32 PadTdl;
    volatile UINT32 Pad29;
    volatile UINT32 PadTdh;
    volatile UINT32 Pad30;
    volatile UINT32 PadTdt;
    volatile UINT32 Pad31;
    volatile UINT32 PadTidv;
    volatile UINT32 Pad32;
    volatile UINT32 Tbt;
    volatile UINT8 Pad33[0x0C];

    volatile UINT32 Ait;
    volatile UINT8 Pad34[0xA4];

    volatile UINT32 Ftr[8];
    volatile UINT32 Fcr;
    volatile UINT32 Pad35;
    volatile UINT32 Trcr;

    volatile UINT8 Pad36[0xD4];

    volatile UINT32 Vfta[(128)];
    volatile UINT8 Pad37[0x700];
    volatile UINT32 Circ;
    volatile UINT8 Pad37a[0xFC];

    volatile UINT32 Pba;
    volatile UINT8 Pad38[0xFFC];

    volatile UINT8 Pad39[0x8];
    volatile UINT32 Ert;
    volatile UINT8 Pad40[0xf4];

    volatile UINT8 Pad41[0x60];
    volatile UINT32 Fcrtl;
    volatile UINT32 Pad42;
    volatile UINT32 Fcrth;
    volatile UINT8 Pad43[0x294];

    volatile UINT8 Pad44[0x10];
    volatile UINT32 Rdfh;
    volatile UINT32 Pad45;
    volatile UINT32 Rdft;
    volatile UINT32 Pad45a;
    volatile UINT32 Rdfhs;
    volatile UINT32 Pad45b;
    volatile UINT32 Rdfts;
    volatile UINT32 Pad45c;
    volatile UINT32 Rdfpc;
    volatile UINT8 Pad46[0x3cc];

    volatile UINT32 Rdbal0;
    volatile UINT32 Rdbah0;
    volatile UINT32 Rdlen0;
    volatile UINT32 Pad47;
    volatile UINT32 Rdh0;
    volatile UINT32 Pad48;
    volatile UINT32 Rdt0;
    volatile UINT32 Pad49;
    volatile UINT32 Rdtr0;
    volatile UINT32 Pad50;
    volatile UINT32 Rxdctl;
    volatile UINT32 Pad51;
    volatile UINT32 Rddh0;
    volatile UINT32 Pad52;
    volatile UINT32 Rddt0;
    volatile UINT8 Pad53[0x7C4];

    volatile UINT32 Txdmac;
    volatile UINT32 Pad54;
    volatile UINT32 Ett;
    volatile UINT8 Pad55[0x3f4];

    volatile UINT8 Pad56[0x10];
    volatile UINT32 Tdfh;
    volatile UINT32 Pad57;
    volatile UINT32 Tdft;
    volatile UINT32 Pad57a;
    volatile UINT32 Tdfhs;
    volatile UINT32 Pad57b;
    volatile UINT32 Tdfts;
    volatile UINT32 Pad57c;
    volatile UINT32 Tdfpc;
    volatile UINT8 Pad58[0x3cc];

    volatile UINT32 Tdbal;
    volatile UINT32 Tdbah;
    volatile UINT32 Tdl;
    volatile UINT32 Pad59;
    volatile UINT32 Tdh;
    volatile UINT32 Pad60;
    volatile UINT32 Tdt;
    volatile UINT32 Pad61;
    volatile UINT32 Tidv;
    volatile UINT32 Pad62;
    volatile UINT32 Txdctl;
    volatile UINT32 Pad63;
    volatile UINT32 Tddh;
    volatile UINT32 Pad64;
    volatile UINT32 Tddt;
    volatile UINT8 Pad65[0x7C4];

    volatile UINT32 Crcerrs;
    volatile UINT32 Algnerrc;
    volatile UINT32 Symerrs;
    volatile UINT32 Rxerrc;
    volatile UINT32 Mpc;
    volatile UINT32 Scc;
    volatile UINT32 Ecol;
    volatile UINT32 Mcc;
    volatile UINT32 Latecol;
    volatile UINT32 Pad66;
    volatile UINT32 Colc;
    volatile UINT32 Tuc;
    volatile UINT32 Dc;
    volatile UINT32 Tncrs;
    volatile UINT32 Sec;
    volatile UINT32 Cexterr;
    volatile UINT32 Rlec;
    volatile UINT32 Rutec;
    volatile UINT32 Xonrxc;
    volatile UINT32 Xontxc;
    volatile UINT32 Xoffrxc;
    volatile UINT32 Xofftxc;
    volatile UINT32 Fcruc;
    volatile UINT32 Prc64;
    volatile UINT32 Prc127;
    volatile UINT32 Prc255;
    volatile UINT32 Prc511;
    volatile UINT32 Prc1023;
    volatile UINT32 Prc1522;
    volatile UINT32 Gprc;
    volatile UINT32 Bprc;
    volatile UINT32 Mprc;
    volatile UINT32 Gptc;
    volatile UINT32 Pad67;
    volatile UINT32 Gorl;
    volatile UINT32 Gorh;
    volatile UINT32 Gotl;
    volatile UINT32 Goth;
    volatile UINT8 Pad68[8];
    volatile UINT32 Rnbc;
    volatile UINT32 Ruc;
    volatile UINT32 Rfc;
    volatile UINT32 Roc;
    volatile UINT32 Rjc;
    volatile UINT8 Pad69[0xC];
    volatile UINT32 Torl;
    volatile UINT32 Torh;
    volatile UINT32 Totl;
    volatile UINT32 Toth;
    volatile UINT32 Tpr;
    volatile UINT32 Tpt;
    volatile UINT32 Ptc64;
    volatile UINT32 Ptc127;
    volatile UINT32 Ptc255;
    volatile UINT32 Ptc511;
    volatile UINT32 Ptc1023;
    volatile UINT32 Ptc1522;
    volatile UINT32 Mptc;
    volatile UINT32 Bptc;

    volatile UINT32 Tsctc;
    volatile UINT32 Tsctfc;
    volatile UINT8 Pad70[0x0F00];

    volatile UINT32 Rxcsum;
    volatile UINT8 Pad71[0x07FC];

    volatile UINT32 Wuc;
    volatile UINT32 Pad72;
    volatile UINT32 Wufc;
    volatile UINT32 Pad73;
    volatile UINT32 Wus;
    volatile UINT8 Pad74[0x24];
    volatile UINT32 Ipav;
    volatile UINT32 Pad75;
    IPAT_ENTRY Ipat[(4)];
    volatile UINT8 Pad76[0xA0];
    volatile UINT32 Wupl;
    volatile UINT8 Pad77[0xFC];
    volatile UINT8 Wupm[0x80];
    volatile UINT8 Pad78[0x480];
    FFLT_ENTRY Fflt[(4)];
    volatile UINT8 Pad79[0x20E0];

    volatile UINT32 PadRdfh;
    volatile UINT32 Pad80;
    volatile UINT32 PadRdft;
    volatile UINT32 Pad81;
    volatile UINT32 PadTdfh;
    volatile UINT32 Pad82;
    volatile UINT32 PadTdft;
    volatile UINT8 Pad83[0xFE4];

    FFMT_ENTRY Ffmt[(128)];
    volatile UINT8 Pad84[0x0400];
    FFVT_ENTRY Ffvt[(128)];

    volatile UINT8 Pad85[0x6400];

    volatile UINT32 Pbm[0x4000];

} E1000_REGISTERS, *PE1000_REGISTERS;

typedef struct _OLD_REGISTERS {

    volatile UINT32 Ctrl;
    volatile UINT32 Pad1;
    volatile UINT32 Status;
    volatile UINT32 Pad2;
    volatile UINT32 Eecd;
    volatile UINT32 Pad3;
    volatile UINT32 Exct;
    volatile UINT32 Pad4;
    volatile UINT32 Mdic;
    volatile UINT32 Pad5;
    volatile UINT32 Fcal;
    volatile UINT32 Fcah;
    volatile UINT32 Fct;
    volatile UINT32 Pad6;

    volatile UINT32 Vet;
    volatile UINT32 Pad7;

    RECEIVE_ADDRESS_REGISTER_PAIR Rar[16];

    volatile UINT32 Icr;
    volatile UINT32 Pad8;
    volatile UINT32 Ics;
    volatile UINT32 Pad9;
    volatile UINT32 Ims;
    volatile UINT32 Pad10;
    volatile UINT32 Imc;
    volatile UINT8 Pad11[0x24];

    volatile UINT32 Rctl;
    volatile UINT32 Pad12;
    volatile UINT32 Rdtr0;
    volatile UINT32 Pad13;
    volatile UINT32 Rdbal0;
    volatile UINT32 Rdbah0;
    volatile UINT32 Rdlen0;
    volatile UINT32 Pad14;
    volatile UINT32 Rdh0;
    volatile UINT32 Pad15;
    volatile UINT32 Rdt0;
    volatile UINT32 Pad16;
    volatile UINT32 Rdtr1;
    volatile UINT32 Pad17;
    volatile UINT32 Rdbal1;
    volatile UINT32 Rdbah1;
    volatile UINT32 Rdlen1;
    volatile UINT32 Pad18;
    volatile UINT32 Rdh1;
    volatile UINT32 Pad19;
    volatile UINT32 Rdt1;
    volatile UINT8 Pad20[0x0C];
    volatile UINT32 Fcrth;
    volatile UINT32 Pad21;
    volatile UINT32 Fcrtl;
    volatile UINT32 Pad22;
    volatile UINT32 Fcttv;
    volatile UINT32 Pad23;
    volatile UINT32 Txcw;
    volatile UINT32 Pad24;
    volatile UINT32 Rxcw;
    volatile UINT8 Pad25[0x7C];
    volatile UINT32 Mta[(128)];

    volatile UINT32 Tctl;
    volatile UINT32 Pad26;
    volatile UINT32 Tqsal;
    volatile UINT32 Tqsah;
    volatile UINT32 Tipg;
    volatile UINT32 Pad27;
    volatile UINT32 Tqc;
    volatile UINT32 Pad28;
    volatile UINT32 Tdbal;
    volatile UINT32 Tdbah;
    volatile UINT32 Tdl;
    volatile UINT32 Pad29;
    volatile UINT32 Tdh;
    volatile UINT32 Pad30;
    volatile UINT32 Tdt;
    volatile UINT32 Pad31;
    volatile UINT32 Tidv;
    volatile UINT32 Pad32;
    volatile UINT32 Tbt;
    volatile UINT8 Pad33[0x0C];

    volatile UINT32 Ait;
    volatile UINT8 Pad34[0xA4];

    volatile UINT32 Ftr[8];
    volatile UINT32 Fcr;
    volatile UINT32 Pad35;
    volatile UINT32 Trcr;

    volatile UINT8 Pad36[0xD4];

    volatile UINT32 Vfta[(128)];
    volatile UINT8 Pad37[0x700];
    volatile UINT32 Circ;
    volatile UINT8 Pad37a[0xFC];

    volatile UINT32 Pba;
    volatile UINT8 Pad38[0xFFC];

    volatile UINT8 Pad39[0x8];
    volatile UINT32 Ert;
    volatile UINT8 Pad40[0x1C];
    volatile UINT32 Rxdctl;
    volatile UINT8 Pad41[0xFD4];

    volatile UINT32 Txdmac;
    volatile UINT32 Pad42;
    volatile UINT32 Ett;
    volatile UINT8 Pad43[0x1C];
    volatile UINT32 Txdctl;
    volatile UINT8 Pad44[0xFD4];

    volatile UINT32 Crcerrs;
    volatile UINT32 Algnerrc;
    volatile UINT32 Symerrs;
    volatile UINT32 Rxerrc;
    volatile UINT32 Mpc;
    volatile UINT32 Scc;
    volatile UINT32 Ecol;
    volatile UINT32 Mcc;
    volatile UINT32 Latecol;
    volatile UINT32 Pad45;
    volatile UINT32 Colc;
    volatile UINT32 Tuc;
    volatile UINT32 Dc;
    volatile UINT32 Tncrs;
    volatile UINT32 Sec;
    volatile UINT32 Cexterr;
    volatile UINT32 Rlec;
    volatile UINT32 Rutec;
    volatile UINT32 Xonrxc;
    volatile UINT32 Xontxc;
    volatile UINT32 Xoffrxc;
    volatile UINT32 Xofftxc;
    volatile UINT32 Fcruc;
    volatile UINT32 Prc64;
    volatile UINT32 Prc127;
    volatile UINT32 Prc255;
    volatile UINT32 Prc511;
    volatile UINT32 Prc1023;
    volatile UINT32 Prc1522;
    volatile UINT32 Gprc;
    volatile UINT32 Bprc;
    volatile UINT32 Mprc;
    volatile UINT32 Gptc;
    volatile UINT32 Pad46;
    volatile UINT32 Gorl;
    volatile UINT32 Gorh;
    volatile UINT32 Gotl;
    volatile UINT32 Goth;
    volatile UINT8 Pad47[8];
    volatile UINT32 Rnbc;
    volatile UINT32 Ruc;
    volatile UINT32 Rfc;
    volatile UINT32 Roc;
    volatile UINT32 Rjc;
    volatile UINT8 Pad48[0xC];
    volatile UINT32 Torl;
    volatile UINT32 Torh;
    volatile UINT32 Totl;
    volatile UINT32 Toth;
    volatile UINT32 Tpr;
    volatile UINT32 Tpt;
    volatile UINT32 Ptc64;
    volatile UINT32 Ptc127;
    volatile UINT32 Ptc255;
    volatile UINT32 Ptc511;
    volatile UINT32 Ptc1023;
    volatile UINT32 Ptc1522;
    volatile UINT32 Mptc;
    volatile UINT32 Bptc;

    volatile UINT32 Tsctc;
    volatile UINT32 Tsctfc;
    volatile UINT8 Pad49[0x0F00];

    volatile UINT32 Rxcsum;
    volatile UINT8 Pad50[0x07FC];

    volatile UINT32 Wuc;
    volatile UINT32 Pad51;
    volatile UINT32 Wufc;
    volatile UINT32 Pad52;
    volatile UINT32 Wus;
    volatile UINT8 Pad53[0x24];
    volatile UINT32 Ipav;
    volatile UINT32 Pad54;
    IPAT_ENTRY Ipat[(4)];
    volatile UINT8 Pad55[0xA0];
    volatile UINT32 Wupl;
    volatile UINT8 Pad56[0xFC];
    volatile UINT8 Wupm[0x80];
    volatile UINT8 Pad57[0x480];
    FFLT_ENTRY Fflt[(4)];
    volatile UINT8 Pad58[0x20E0];

    volatile UINT32 Rdfh;
    volatile UINT32 Pad59;
    volatile UINT32 Rdft;
    volatile UINT32 Pad60;
    volatile UINT32 Tdfh;
    volatile UINT32 Pad61;
    volatile UINT32 Tdft;
    volatile UINT32 Pad62;
    volatile UINT32 Tdfhs;
    volatile UINT32 Pad63;
    volatile UINT32 Tdfts;
    volatile UINT32 Pad64;
    volatile UINT32 Tdfpc;
    volatile UINT8 Pad65[0x0FCC];

    FFMT_ENTRY Ffmt[(128)];
    volatile UINT8 Pad66[0x0400];
    FFVT_ENTRY Ffvt[(128)];

    volatile UINT8 Pad67[0x6400];

    volatile UINT32 Pbm[0x4000];

} OLD_REGISTERS, *POLD_REGISTERS;

#define E1000_EEPROM_SWDPIN0       (0x00000001)
#define E1000_EEPROM_LED_LOGIC     (0x0020)

#define E1000_CTRL_FD              (0x00000001)
#define E1000_CTRL_BEM             (0x00000002)
#define E1000_CTRL_PRIOR           (0x00000004)
#define E1000_CTRL_LRST            (0x00000008)
#define E1000_CTRL_TME             (0x00000010)
#define E1000_CTRL_SLE             (0x00000020)
#define E1000_CTRL_ASDE            (0x00000020)
#define E1000_CTRL_SLU             (0x00000040)

#define E1000_CTRL_ILOS            (0x00000080)
#define E1000_CTRL_SPD_SEL         (0x00000300)
#define E1000_CTRL_SPD_10          (0x00000000)
#define E1000_CTRL_SPD_100         (0x00000100)
#define E1000_CTRL_SPD_1000        (0x00000200)
#define E1000_CTRL_BEM32           (0x00000400)
#define E1000_CTRL_FRCSPD          (0x00000800)
#define E1000_CTRL_FRCDPX          (0x00001000)

#define E1000_CTRL_SWDPIN0         (0x00040000)
#define E1000_CTRL_SWDPIN1         (0x00080000)
#define E1000_CTRL_SWDPIN2         (0x00100000)
#define E1000_CTRL_SWDPIN3         (0x00200000)
#define E1000_CTRL_SWDPIO0         (0x00400000)
#define E1000_CTRL_SWDPIO1         (0x00800000)
#define E1000_CTRL_SWDPIO2         (0x01000000)
#define E1000_CTRL_SWDPIO3         (0x02000000)
#define E1000_CTRL_RST             (0x04000000)
#define E1000_CTRL_RFCE            (0x08000000)
#define E1000_CTRL_TFCE            (0x10000000)

#define E1000_CTRL_RTE             (0x20000000)
#define E1000_CTRL_VME             (0x40000000)

#define E1000_CTRL_PHY_RST         (0x80000000)

#define E1000_STATUS_FD            (0x00000001)
#define E1000_STATUS_LU            (0x00000002)
#define E1000_STATUS_TCKOK         (0x00000004)
#define E1000_STATUS_RBCOK         (0x00000008)
#define E1000_STATUS_TXOFF         (0x00000010)
#define E1000_STATUS_TBIMODE       (0x00000020)
#define E1000_STATUS_SPEED_10      (0x00000000)
#define E1000_STATUS_SPEED_100     (0x00000040)
#define E1000_STATUS_SPEED_1000    (0x00000080)
#define E1000_STATUS_ASDV          (0x00000300)
#define E1000_STATUS_MTXCKOK       (0x00000400)
#define E1000_STATUS_PCI66         (0x00000800)
#define E1000_STATUS_BUS64         (0x00001000)
#define E1000_STATUS_PCIX_MODE     (0x00002000)
#define E1000_STATUS_PCIX_SPEED    (0x0000C000)

#define E1000_STATUS_PCIX_SPEED_66  (0x00000000)
#define E1000_STATUS_PCIX_SPEED_100 (0x00004000)
#define E1000_STATUS_PCIX_SPEED_133 (0x00008000)

#define E1000_EESK                 (0x00000001)
#define E1000_EECS                 (0x00000002)
#define E1000_EEDI                 (0x00000004)
#define E1000_EEDO                 (0x00000008)
#define E1000_FLASH_WRITE_DIS      (0x00000010)
#define E1000_FLASH_WRITE_EN       (0x00000020)

#define E1000_EXCTRL_GPI_EN0       (0x00000001)
#define E1000_EXCTRL_GPI_EN1       (0x00000002)
#define E1000_EXCTRL_GPI_EN2       (0x00000004)
#define E1000_EXCTRL_GPI_EN3       (0x00000008)
#define E1000_EXCTRL_SWDPIN4       (0x00000010)
#define E1000_EXCTRL_SWDPIN5       (0x00000020)
#define E1000_EXCTRL_SWDPIN6       (0x00000040)
#define E1000_EXCTRL_SWDPIN7       (0x00000080)
#define E1000_EXCTRL_SWDPIO4       (0x00000100)
#define E1000_EXCTRL_SWDPIO5       (0x00000200)
#define E1000_EXCTRL_SWDPIO6       (0x00000400)
#define E1000_EXCTRL_SWDPIO7       (0x00000800)
#define E1000_EXCTRL_ASDCHK        (0x00001000)
#define E1000_EXCTRL_EE_RST        (0x00002000)
#define E1000_EXCTRL_IPS           (0x00004000)
#define E1000_EXCTRL_SPD_BYPS      (0x00008000)

#define E1000_MDI_WRITE            (0x04000000)
#define E1000_MDI_READ             (0x08000000)
#define E1000_MDI_READY            (0x10000000)
#define E1000_MDI_INT              (0x20000000)
#define E1000_MDI_ERR              (0x40000000)

#define E1000_RAH_RDR              (0x40000000)
#define E1000_RAH_AV               (0x80000000)

#define E1000_ICR_TXDW             (0x00000001)
#define E1000_ICR_TXQE             (0x00000002)
#define E1000_ICR_LSC              (0x00000004)
#define E1000_ICR_RXSEQ            (0x00000008)
#define E1000_ICR_RXDMT0           (0x00000010)
#define E1000_ICR_RXDMT1           (0x00000020)
#define E1000_ICR_RXO              (0x00000040)
#define E1000_ICR_RXT0             (0x00000080)
#define E1000_ICR_RXT1             (0x00000100)
#define E1000_ICR_MDAC             (0x00000200)
#define E1000_ICR_RXCFG            (0x00000400)
#define E1000_ICR_GPI_EN0          (0x00000800)
#define E1000_ICR_GPI_EN1          (0x00001000)
#define E1000_ICR_GPI_EN2          (0x00002000)
#define E1000_ICR_GPI_EN3          (0x00004000)

#define E1000_ICS_TXDW             E1000_ICR_TXDW
#define E1000_ICS_TXQE             E1000_ICR_TXQE
#define E1000_ICS_LSC              E1000_ICR_LSC
#define E1000_ICS_RXSEQ            E1000_ICR_RXSEQ
#define E1000_ICS_RXDMT0           E1000_ICR_RXDMT0
#define E1000_ICS_RXDMT1           E1000_ICR_RXDMT1
#define E1000_ICS_RXO              E1000_ICR_RXO
#define E1000_ICS_RXT0             E1000_ICR_RXT0
#define E1000_ICS_RXT1             E1000_ICR_RXT1
#define E1000_ICS_MDAC             E1000_ICR_MDAC
#define E1000_ICS_RXCFG            E1000_ICR_RXCFG
#define E1000_ICS_GPI_EN0          E1000_ICR_GPI_EN0
#define E1000_ICS_GPI_EN1          E1000_ICR_GPI_EN1
#define E1000_ICS_GPI_EN2          E1000_ICR_GPI_EN2
#define E1000_ICS_GPI_EN3          E1000_ICR_GPI_EN3

#define E1000_IMS_TXDW             E1000_ICR_TXDW
#define E1000_IMS_TXQE             E1000_ICR_TXQE
#define E1000_IMS_LSC              E1000_ICR_LSC
#define E1000_IMS_RXSEQ            E1000_ICR_RXSEQ
#define E1000_IMS_RXDMT0           E1000_ICR_RXDMT0
#define E1000_IMS_RXDMT1           E1000_ICR_RXDMT1
#define E1000_IMS_RXO              E1000_ICR_RXO
#define E1000_IMS_RXT0             E1000_ICR_RXT0
#define E1000_IMS_RXT1             E1000_ICR_RXT1
#define E1000_IMS_MDAC             E1000_ICR_MDAC
#define E1000_IMS_RXCFG            E1000_ICR_RXCFG
#define E1000_IMS_GPI_EN0          E1000_ICR_GPI_EN0
#define E1000_IMS_GPI_EN1          E1000_ICR_GPI_EN1
#define E1000_IMS_GPI_EN2          E1000_ICR_GPI_EN2
#define E1000_IMS_GPI_EN3          E1000_ICR_GPI_EN3

#define E1000_IMC_TXDW             E1000_ICR_TXDW
#define E1000_IMC_TXQE             E1000_ICR_TXQE
#define E1000_IMC_LSC              E1000_ICR_LSC
#define E1000_IMC_RXSEQ            E1000_ICR_RXSEQ
#define E1000_IMC_RXDMT0           E1000_ICR_RXDMT0
#define E1000_IMC_RXDMT1           E1000_ICR_RXDMT1
#define E1000_IMC_RXO              E1000_ICR_RXO
#define E1000_IMC_RXT0             E1000_ICR_RXT0
#define E1000_IMC_RXT1             E1000_ICR_RXT1
#define E1000_IMC_MDAC             E1000_ICR_MDAC
#define E1000_IMC_RXCFG            E1000_ICR_RXCFG
#define E1000_IMC_GPI_EN0          E1000_ICR_GPI_EN0
#define E1000_IMC_GPI_EN1          E1000_ICR_GPI_EN1
#define E1000_IMC_GPI_EN2          E1000_ICR_GPI_EN2
#define E1000_IMC_GPI_EN3          E1000_ICR_GPI_EN3

#define E1000_TINT_RINT_PCI        (E1000_TXDW|E1000_ICR_RXT0)
#define E1000_CAUSE_ERR            (E1000_ICR_RXSEQ|E1000_ICR_RXO)

#define E1000_RCTL_RST             (0x00000001)
#define E1000_RCTL_EN              (0x00000002)
#define E1000_RCTL_SBP             (0x00000004)
#define E1000_RCTL_UPE             (0x00000008)
#define E1000_RCTL_MPE             (0x00000010)
#define E1000_RCTL_LPE             (0x00000020)
#define E1000_RCTL_LBM_NO          (0x00000000)
#define E1000_RCTL_LBM_MAC         (0x00000040)
#define E1000_RCTL_LBM_SLP         (0x00000080)
#define E1000_RCTL_LBM_TCVR        (0x000000c0)
#define E1000_RCTL_RDMTS0_HALF     (0x00000000)
#define E1000_RCTL_RDMTS0_QUAT     (0x00000100)
#define E1000_RCTL_RDMTS0_EIGTH    (0x00000200)
#define E1000_RCTL_RDMTS1_HALF     (0x00000000)
#define E1000_RCTL_RDMTS1_QUAT     (0x00000400)
#define E1000_RCTL_RDMTS1_EIGTH    (0x00000800)
#define E1000_RCTL_MO_SHIFT        12

#define E1000_RCTL_MO_0            (0x00000000)
#define E1000_RCTL_MO_1            (0x00001000)
#define E1000_RCTL_MO_2            (0x00002000)
#define E1000_RCTL_MO_3            (0x00003000)

#define E1000_RCTL_MDR             (0x00004000)
#define E1000_RCTL_BAM             (0x00008000)

#define E1000_RCTL_SZ_2048         (0x00000000)
#define E1000_RCTL_SZ_1024         (0x00010000)
#define E1000_RCTL_SZ_512          (0x00020000)
#define E1000_RCTL_SZ_256          (0x00030000)

#define E1000_RCTL_SZ_16384        (0x00010000)
#define E1000_RCTL_SZ_8192         (0x00020000)
#define E1000_RCTL_SZ_4096         (0x00030000)

#define E1000_RCTL_VFE             (0x00040000)

#define E1000_RCTL_CFIEN           (0x00080000)
#define E1000_RCTL_CFI             (0x00100000)
#define E1000_RCTL_ISLE            (0x00200000)

#define E1000_RCTL_DPF             (0x00400000)
#define E1000_RCTL_PMCF            (0x00800000)

#define E1000_RCTL_SISLH           (0x01000000)

#define E1000_RCTL_BSEX            (0x02000000)
#define E1000_RDT0_DELAY           (0x0000ffff)
#define E1000_RDT0_FPDB            (0x80000000)

#define E1000_RDT1_DELAY           (0x0000ffff)
#define E1000_RDT1_FPDB            (0x80000000)

#define E1000_RDLEN0_LEN           (0x0007ff80)

#define E1000_RDLEN1_LEN           (0x0007ff80)

#define E1000_RDH0_RDH             (0x0000ffff)

#define E1000_RDH1_RDH             (0x0000ffff)

#define E1000_RDT0_RDT             (0x0000ffff)

#define E1000_FCRTH_RTH            (0x0000FFF8)
#define E1000_FCRTH_XFCE           (0x80000000)

#define E1000_FCRTL_RTL            (0x0000FFF8)
#define E1000_FCRTL_XONE           (0x80000000)

#define E1000_RXDCTL_PTHRESH       0x0000003F
#define E1000_RXDCTL_HTHRESH       0x00003F00
#define E1000_RXDCTL_WTHRESH       0x003F0000
#define E1000_RXDCTL_GRAN          0x01000000

#define E1000_TXDCTL_PTHRESH       0x000000FF
#define E1000_TXDCTL_HTHRESH       0x0000FF00
#define E1000_TXDCTL_WTHRESH       0x00FF0000
#define E1000_TXDCTL_GRAN          0x01000000

#define E1000_TXCW_FD              (0x00000020)
#define E1000_TXCW_HD              (0x00000040)
#define E1000_TXCW_PAUSE           (0x00000080)
#define E1000_TXCW_ASM_DIR         (0x00000100)
#define E1000_TXCW_PAUSE_MASK      (0x00000180)
#define E1000_TXCW_RF              (0x00003000)
#define E1000_TXCW_NP              (0x00008000)
#define E1000_TXCW_CW              (0x0000ffff)
#define E1000_TXCW_TXC             (0x40000000)
#define E1000_TXCW_ANE             (0x80000000)

#define E1000_RXCW_CW              (0x0000ffff)
#define E1000_RXCW_NC              (0x04000000)
#define E1000_RXCW_IV              (0x08000000)
#define E1000_RXCW_CC              (0x10000000)
#define E1000_RXCW_C               (0x20000000)
#define E1000_RXCW_SYNCH           (0x40000000)
#define E1000_RXCW_ANC             (0x80000000)

#define E1000_TCTL_RST             (0x00000001)
#define E1000_TCTL_EN              (0x00000002)
#define E1000_TCTL_BCE             (0x00000004)
#define E1000_TCTL_PSP             (0x00000008)
#define E1000_TCTL_CT              (0x00000ff0)
#define E1000_TCTL_COLD            (0x003ff000)
#define E1000_TCTL_SWXOFF          (0x00400000)
#define E1000_TCTL_PBE             (0x00800000)
#define E1000_TCTL_RTLC            (0x01000000)
#define E1000_TCTL_NRTU            (0x02000000)

#define E1000_TQSAL_TQSAL          (0xffffffc0)
#define E1000_TQSAH_TQSAH          (0xffffffff)

#define E1000_TQC_SQ               (0x00000001)
#define E1000_TQC_RQ               (0x00000002)

#define E1000_TDBAL_TDBAL          (0xfffff000)
#define E1000_TDBAH_TDBAH          (0xffffffff)

#define E1000_TDL_LEN              (0x0007ff80)

#define E1000_TDH_TDH              (0x0000ffff)

#define E1000_TDT_TDT              (0x0000ffff)

#define E1000_RXCSUM_PCSS          (0x000000ff)
#define E1000_RXCSUM_IPOFL         (0x00000100)
#define E1000_RXCSUM_TUOFL         (0x00000200)

#define E1000_WUC_APME             (0x00000001)
#define E1000_WUC_PME_EN           (0x00000002)
#define E1000_WUC_PME_STATUS       (0x00000004)
#define E1000_WUC_APMPME           (0x00000008)

#define E1000_WUFC_LNKC            (0x00000001)
#define E1000_WUFC_MAG             (0x00000002)
#define E1000_WUFC_EX              (0x00000004)
#define E1000_WUFC_MC              (0x00000008)
#define E1000_WUFC_BC              (0x00000010)
#define E1000_WUFC_ARP             (0x00000020)
#define E1000_WUFC_IP              (0x00000040)
#define E1000_WUFC_FLX0            (0x00010000)
#define E1000_WUFC_FLX1            (0x00020000)
#define E1000_WUFC_FLX2            (0x00040000)
#define E1000_WUFC_FLX3            (0x00080000)
#define E1000_WUFC_ALL_FILTERS     (0x000F007F)

#define E1000_WUFC_FLX_OFFSET      (16)
#define E1000_WUFC_FLX_FILTERS     (0x000F0000)

#define E1000_WUS_LNKC             (0x00000001)
#define E1000_WUS_MAG              (0x00000002)
#define E1000_WUS_EX               (0x00000004)
#define E1000_WUS_MC               (0x00000008)
#define E1000_WUS_BC               (0x00000010)
#define E1000_WUS_ARP              (0x00000020)
#define E1000_WUS_IP               (0x00000040)
#define E1000_WUS_FLX0             (0x00010000)
#define E1000_WUS_FLX1             (0x00020000)
#define E1000_WUS_FLX2             (0x00040000)
#define E1000_WUS_FLX3             (0x00080000)
#define E1000_WUS_FLX_FILTERS      (0x000F0000)

#define E1000_WUPL_LENGTH_MASK     (0x0FFF)

/* use old register MacType < MAC_LIVENGOOD */

#if MACTYPE >= MAC_LIVENGOOD

#  define E1000_WRITE_REG(reg, value) (writel(value, &((PE1000_REGISTERS)ioaddr)->reg) )
#  define E1000_READ_REG(reg) (readl(&((PE1000_REGISTERS)ioaddr)->reg))

#else

#  define E1000_WRITE_REG(reg, value) (writel(value, &((POLD_REGISTERS)ioaddr)->reg))
#  define E1000_READ_REG(reg) (readl(&((POLD_REGISTERS)ioaddr)->reg))

#endif


#define E1000_MDALIGN               (4096)

#define EEPROM_READ_OPCODE          (0x6)
#define EEPROM_WRITE_OPCODE         (0x5)
#define EEPROM_ERASE_OPCODE         (0x7)
#define EEPROM_EWEN_OPCODE          (0x13)
#define EEPROM_EWDS_OPCODE          (0x10)

#define EEPROM_INIT_CONTROL1_REG    (0x000A)
#define EEPROM_INIT_CONTROL2_REG    (0x000F)
#define EEPROM_CHECKSUM_REG         (0x003F)

#define EEPROM_WORD0A_ILOS          (0x0010)
#define EEPROM_WORD0A_SWDPIO        (0x01E0)
#define EEPROM_WORD0A_LRST          (0x0200)
#define EEPROM_WORD0A_FD            (0x0400)
#define EEPROM_WORD0A_66MHZ         (0x0800)

#define EEPROM_WORD0F_PAUSE_MASK    (0x3000)
#define EEPROM_WORD0F_PAUSE         (0x1000)
#define EEPROM_WORD0F_ASM_DIR       (0x2000)
#define EEPROM_WORD0F_ANE           (0x0800)
#define EEPROM_WORD0F_SWPDIO_EXT    (0x00F0)

#define EEPROM_SUM                  (0xBABA)

#define EEPROM_NODE_ADDRESS_BYTE_0  (0)
#define EEPROM_PBA_BYTE_1           (8)

#define EEPROM_WORD_SIZE            (64)

#define NODE_ADDRESS_SIZE           (6)
#define PBA_SIZE                    (4)

#define E1000_COLLISION_THRESHOLD   16
#define E1000_CT_SHIFT              4

#define E1000_FDX_COLLISION_DISTANCE 64
#define E1000_HDX_COLLISION_DISTANCE 64
#define E1000_GB_HDX_COLLISION_DISTANCE 512
#define E1000_COLD_SHIFT            12

#define REQ_TX_DESCRIPTOR_MULTIPLE  8
#define REQ_RX_DESCRIPTOR_MULTIPLE  8

#define DEFAULT_WSMN_TIPG_IPGT      10

#define DEFAULT_LVGD_TIPG_IPGT_FIBER 6
#define DEFAULT_LVGD_TIPG_IPGT_COPPER 8

#define E1000_TIPG_IPGT_MASK        0x000003FF
#define E1000_TIPG_IPGR1_MASK       0x000FFC00
#define E1000_TIPG_IPGR2_MASK       0x3FF00000

#define DEFAULT_WSMN_TIPG_IPGR1     2
#define DEFAULT_LVGD_TIPG_IPGR1     8
#define E1000_TIPG_IPGR1_SHIFT      10

#define DEFAULT_WSMN_TIPG_IPGR2     10
#define DEFAULT_LVGD_TIPG_IPGR2     6
#define E1000_TIPG_IPGR2_SHIFT      20

#define E1000_TXDMAC_DPP            0x00000001

#define FLOW_CONTROL_ADDRESS_LOW    (0x00C28001)
#define FLOW_CONTROL_ADDRESS_HIGH   (0x00000100)
#define FLOW_CONTROL_TYPE           (0x8808)

#define FC_DEFAULT_HI_THRESH        (0x8000)
#define FC_DEFAULT_LO_THRESH        (0x4000)
#define FC_DEFAULT_TX_TIMER         (0x100)

#define PAUSE_SHIFT 5

#define SWDPIO_SHIFT 17

#define SWDPIO__EXT_SHIFT 4

#define ILOS_SHIFT  3

#define MDI_REGADD_SHIFT 16

#define MDI_PHYADD_SHIFT 21

#define RECEIVE_BUFFER_ALIGN_SIZE  (256)

#define LINK_UP_TIMEOUT             500

#define E1000_TX_BUFFER_SIZE ((UINT)1514)

#define E1000_MIN_SIZE_OF_RECEIVE_BUFFERS (2048)

#define E1000_SIZE_OF_RECEIVE_BUFFERS (2048)

#define E1000_SIZE_OF_UNALIGNED_RECEIVE_BUFFERS E1000_SIZE_OF_RECEIVE_BUFFERS+RECEIVE_BUFFER_ALIGN_SIZE

#define COALESCE_BUFFER_SIZE  0x800
#define COALESCE_BUFFER_ALIGN 0x800

#define CARRIER_EXTENSION   0x0F

#define TBI_ACCEPT(RxErrors, LastByteInFrame, HwFrameLength) (Adapter->TbiCompatibilityOn                                      && (((RxErrors) & E1000_RXD_ERR_FRAME_ERR_MASK) == E1000_RXD_ERR_CE)&& ((LastByteInFrame) == CARRIER_EXTENSION)                         && ((HwFrameLength) > 64)                                           && ((HwFrameLength) <= Adapter->MaxFrameSize+1))

#define E1000_WAIT_PERIOD           10

#endif /* _FXHW_ */

