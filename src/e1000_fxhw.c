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

/* Workfile: fxhw.c  */
/* Revision: 42  */
/* Date: 8/03/01 10:39a  */

#include "e1000.h"

#define IN
#define OUT

VOID AdapterStop(void);

BOOLEAN InitializeHardware(void);

VOID InitRxAddresses(void);

VOID
MulticastAddressListUpdate(PUCHAR MulticastAddressList,
                           UINT32 MulticastAddressCount, UINT32 Padding);

UINT32
HashMulticastAddress(PUCHAR MulticastAddress);

VOID MtaSet(UINT32 HashValue);

VOID
RarSet(PUCHAR MulticastAddress, UINT32 RarIndex);

VOID WriteVfta(UINT32 Offset, UINT32 Value);

VOID ClearVfta(void);

BOOLEAN SetupFlowControlAndLink(void);

BOOLEAN SetupPcsLink(UINT32 DeviceControlReg);

VOID ConfigFlowControlAfterLinkUp(void);

VOID ForceMacFlowControlSetting(void);

VOID CheckForLink(void);

VOID ClearHwStatsCounters(void);

VOID
GetSpeedAndDuplex(PUINT16 Speed, PUINT16 Duplex);

UINT16 ReadEepromWord(UINT16 Reg);

STATIC
    VOID ShiftOutBits(UINT16 Data, UINT16 Count);

STATIC VOID RaiseClock(UINT32 * EecdRegValue);

STATIC VOID LowerClock(UINT32 * EecdRegValue);

STATIC USHORT ShiftInBits(void);

STATIC VOID EepromCleanup(void);

BOOLEAN ValidateEepromChecksum(void);

VOID UpdateEepromChecksum(void);

BOOLEAN WriteEepromWord(USHORT reg, USHORT data);

STATIC UINT16 WaitEepromCommandDone(void);

STATIC VOID StandBy(void);

UINT32 ReadPartNumber(void);

VOID IdLedOn(void);

VOID IdLedOff(void);

VOID SetIdLedForPCIX(void);

BOOLEAN IsLowProfile(void);
#if 1
VOID AdapterStop(void)
{

    UINT32 IcrContents;

    UINT16 tPciCommandWord;

    DEBUGFUNC("AdapterStop")

        if (AdapterStopped) {
        DEBUGOUT("Exiting because the adapter is already stopped!!!\n");
        return;
    }

    AdapterStopped = TRUE;

    if (MacType == MAC_WISEMAN_2_0) {
        if (PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
            DEBUGOUT("Disabling MWI on rev 2.0 Wiseman silicon\n");

            tPciCommandWord =
                PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;

            WritePciConfigWord(PCI_COMMAND_REGISTER, &tPciCommandWord);
        }
    }

    DEBUGOUT("Masking off all interrupts\n");
    E1000_WRITE_REG(Imc, 0xffffffff);

    E1000_WRITE_REG(Rctl, 0);
    E1000_WRITE_REG(Tctl, 0);

    TbiCompatibilityOn = FALSE;

    DelayInMilliseconds(10);

    DEBUGOUT("Issuing a global reset to MAC\n");
    E1000_WRITE_REG(Ctrl, E1000_CTRL_RST);

    DelayInMilliseconds(10);

    DEBUGOUT("Masking off all interrupts\n");
    E1000_WRITE_REG(Imc, 0xffffffff);

    IcrContents = E1000_READ_REG(Icr);

    if (MacType == MAC_WISEMAN_2_0) {
        if (PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
            WritePciConfigWord(PCI_COMMAND_REGISTER,
                               &PciCommandWord);
        }
    }

}
#endif

BOOLEAN InitializeHardware(void)
 {
    UINT i;
    UINT16 tPciCommandWord;
    BOOLEAN Status;
    UINT32 RegisterValue;

    DEBUGFUNC("InitializeHardware");

#if MACTYPE != MAC_LIVENGOOD 
        TbiCompatibilityEnable = FALSE;
#endif

#if (MACTYPE >= MAC_LIVENGOOD)
        RegisterValue = E1000_READ_REG(Status);
        if (RegisterValue & E1000_STATUS_TBIMODE) {
            MediaType = MEDIA_TYPE_FIBER;

            TbiCompatibilityEnable = FALSE;
        } else {
            MediaType = MEDIA_TYPE_COPPER;
        }
#else 

        MediaType = MEDIA_TYPE_FIBER;
#endif 

    DEBUGOUT("Initializing the IEEE VLAN\n");
    E1000_WRITE_REG(Vet, 0);

    ClearVfta();

#if MACTYPE == MAC_WISEMAN_2_0

        if (PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
            DEBUGOUT("Disabling MWI on rev 2.0 Wiseman silicon\n");

            tPciCommandWord =
                PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;

            WritePciConfigWord(PCI_COMMAND_REGISTER, &tPciCommandWord);
        }

        E1000_WRITE_REG(Rctl, E1000_RCTL_RST);

        DelayInMilliseconds(5);
#endif

    InitRxAddresses();

#if MACTYPE == MAC_WISEMAN_2_0
        E1000_WRITE_REG(Rctl, 0);

        DelayInMilliseconds(1);

        if (PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
            WritePciConfigWord(PCI_COMMAND_REGISTER,
                               &PciCommandWord);
        }

#endif

    DEBUGOUT("Zeroing the MTA\n");
    for (i = 0; i < E1000_MC_TBL_SIZE; i++) {
        E1000_WRITE_REG(Mta[i], 0);
    }

    Status = SetupFlowControlAndLink();


    return (Status);
}

VOID InitRxAddresses(void)
 {
    UINT i;
    UINT32 HwLowAddress;
    UINT32 HwHighAddress;

    DEBUGFUNC("InitRxAddresses")

        DEBUGOUT("Programming IA into RAR[0]\n");
    HwLowAddress = (mac_addr[0] |
                    (mac_addr[1] << 8) |
                    (mac_addr[2] << 16) |
                    (mac_addr[3] << 24));

    HwHighAddress = (mac_addr[4] |
                     (mac_addr[5] << 8) | E1000_RAH_AV);

    E1000_WRITE_REG(Rar[0].Low, HwLowAddress);
    E1000_WRITE_REG(Rar[0].High, HwHighAddress);

    DEBUGOUT("Clearing RAR[1-15]\n");
    for (i = 1; i < E1000_RAR_ENTRIES; i++) {
        E1000_WRITE_REG(Rar[i].Low, 0);
        E1000_WRITE_REG(Rar[i].High, 0);
    }

    return;
}

VOID ClearVfta(void)
 {
    UINT32 Offset;

    for (Offset = 0; Offset < E1000_VLAN_FILTER_TBL_SIZE; Offset++)
        E1000_WRITE_REG(Vfta[Offset], 0);

}

BOOLEAN SetupFlowControlAndLink(void)
 {
    UINT32 TempEepromWord;
    UINT32 DeviceControlReg;
    UINT32 ExtDevControlReg;
    BOOLEAN Status = TRUE;

    DEBUGFUNC("SetupFlowControlAndLink")

        TempEepromWord = ReadEepromWord(EEPROM_INIT_CONTROL1_REG);

    DeviceControlReg =
        (((TempEepromWord & EEPROM_WORD0A_SWDPIO) << SWDPIO_SHIFT) |
         ((TempEepromWord & EEPROM_WORD0A_ILOS) << ILOS_SHIFT));
#ifndef USE_COMPLEX_VERSION
    Status = PhySetup(DeviceControlReg);
    E1000_WRITE_REG(Fcrtl, 0);
    E1000_WRITE_REG(Fcrth, 0);
#else
//    if (Adapter->DmaFairness)
//        DeviceControlReg |= E1000_CTRL_PRIOR;
    TempEepromWord = ReadEepromWord(EEPROM_INIT_CONTROL2_REG);

    if (FlowControl > FLOW_CONTROL_FULL) {
        if ((TempEepromWord & EEPROM_WORD0F_PAUSE_MASK) == 0)
            FlowControl = FLOW_CONTROL_NONE;
        else if ((TempEepromWord & EEPROM_WORD0F_PAUSE_MASK) ==
                 EEPROM_WORD0F_ASM_DIR) FlowControl =
                FLOW_CONTROL_TRANSMIT_PAUSE;
        else
            FlowControl = FLOW_CONTROL_FULL;
    }
    OriginalFlowControl = FlowControl;

#if MACTTYPE == MAC_WISEMAN_2_0
        FlowControl &= (~FLOW_CONTROL_TRANSMIT_PAUSE);
#endif

#if ((MACTYPE < MAC_LIVENGOOD))
        if (ReportTxEarly == 1) FlowControl &= (~FLOW_CONTROL_RECEIVE_PAUSE);
//    DEBUGOUT1("After fix-ups FlowControl is now = %x\n",
//              FlowControl);
#endif

#if (MACTYPE >= MAC_LIVENGOOD) 
        ExtDevControlReg = ((TempEepromWord & EEPROM_WORD0F_SWPDIO_EXT)
                            << SWDPIO__EXT_SHIFT);
        E1000_WRITE_REG(Exct, ExtDevControlReg);
#endif

#if (MACTYPE >= MAC_LIVENGOOD)
#        ifdef USE_FIBER
            Status = SetupPcsLink(DeviceControlReg);
#        else
            Status = PhySetup(DeviceControlReg);
#	endif 
#else 
        Status = SetupPcsLink(DeviceControlReg);
#endif 

    
    DEBUGOUT
        ("Initializing the Flow Control address, type and timer regs\n");

    E1000_WRITE_REG(Fcal, FLOW_CONTROL_ADDRESS_LOW);
    E1000_WRITE_REG(Fcah, FLOW_CONTROL_ADDRESS_HIGH);
    E1000_WRITE_REG(Fct, FLOW_CONTROL_TYPE);
    E1000_WRITE_REG(Fcttv, FlowControlPauseTime);

    if (!(FlowControl & FLOW_CONTROL_TRANSMIT_PAUSE)) {
        E1000_WRITE_REG(Fcrtl, 0);
        E1000_WRITE_REG(Fcrth, 0);
    } else {

        if (FlowControlSendXon) {
            E1000_WRITE_REG(Fcrtl,
                            ( FlowControlLowWatermark | E1000_FCRTL_XONE));
            E1000_WRITE_REG(Fcrth, FlowControlHighWatermark);
        } else {
            E1000_WRITE_REG(Fcrtl, FlowControlLowWatermark);
            E1000_WRITE_REG(Fcrth, FlowControlHighWatermark);
        }
    }

#endif
    return (Status);
}

#ifdef USE_FIBER
BOOLEAN SetupPcsLink(UINT32 DeviceControlReg)
 {
    UINT32 i;
    UINT32 StatusContents;
    UINT32 TctlReg;
    UINT32 TransmitConfigWord=0;
    UINT32 Shift32;

    DEBUGFUNC("SetupPcsLink")

        TctlReg = E1000_READ_REG(Tctl);
    Shift32 = E1000_FDX_COLLISION_DISTANCE;
    Shift32 <<= E1000_COLD_SHIFT;
    TctlReg |= Shift32;
    E1000_WRITE_REG(Tctl, TctlReg);

    switch (FlowControl) {
    case FLOW_CONTROL_NONE:

        TransmitConfigWord = (E1000_TXCW_ANE | E1000_TXCW_FD);

        break;

    case FLOW_CONTROL_RECEIVE_PAUSE:

        TransmitConfigWord =
            (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);

        break;

    case FLOW_CONTROL_TRANSMIT_PAUSE:

        TransmitConfigWord =
            (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);

        break;

    case FLOW_CONTROL_FULL:

        TransmitConfigWord =
            (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);

        break;

    default:

        DEBUGOUT("Flow control param set incorrectly\n");
        ASSERT(0);
        break;
    }

    DEBUGOUT("Auto-negotiation enabled\n");

    E1000_WRITE_REG(Txcw, TransmitConfigWord);
    E1000_WRITE_REG(Ctrl, DeviceControlReg);

    TxcwRegValue = TransmitConfigWord;
    DelayInMilliseconds(1);

    if (!(E1000_READ_REG(Ctrl) & E1000_CTRL_SWDPIN1)) {

        DEBUGOUT("Looking for Link\n");
        for (i = 0; i < (LINK_UP_TIMEOUT / 10); i++) {
            DelayInMilliseconds(10);

            StatusContents = E1000_READ_REG(Status);
            if (StatusContents & E1000_STATUS_LU)
                break;
        }

        if (i == (LINK_UP_TIMEOUT / 10)) {

            DEBUGOUT("Never got a valid link from auto-neg!!!\n");

            AutoNegFailed = 1;
            CheckForLink();
            AutoNegFailed = 0;
        } else {
            AutoNegFailed = 0;
            DEBUGOUT("Valid Link Found\n");
        }
    } else {
        DEBUGOUT("No Signal Detected\n");
    }

    return (TRUE);
}
#endif


#ifdef USE_COMPLEX_VERSION
VOID ConfigFlowControlAfterLinkUp(void)
 {
    UINT16 MiiStatusReg, MiiNWayAdvertiseReg, MiiNWayBasePgAbleReg;
    UINT16 Speed, Duplex;

    DEBUGFUNC("ConfigFlowControlAfterLinkUp")

        if (
            ((MediaType == MEDIA_TYPE_FIBER)
             && (AutoNegFailed))
            || ((MediaType == MEDIA_TYPE_COPPER)
                && (!AutoNeg))) {
        ForceMacFlowControlSetting();
    }

    if ((MediaType == MEDIA_TYPE_COPPER) && AutoNeg) {

        MiiStatusReg = ReadPhyRegister(
                                       PHY_MII_STATUS_REG,
                                       PhyAddress);

        MiiStatusReg = ReadPhyRegister(
                                       PHY_MII_STATUS_REG,
                                       PhyAddress);

        if (MiiStatusReg & MII_SR_AUTONEG_COMPLETE) {

            MiiNWayAdvertiseReg = ReadPhyRegister(
                                                  PHY_AUTONEG_ADVERTISEMENT,
                                                  PhyAddress);

            MiiNWayBasePgAbleReg = ReadPhyRegister(
                                                   PHY_AUTONEG_LP_BPA,
                                                   PhyAddress);

            if ((MiiNWayAdvertiseReg & NWAY_AR_PAUSE) &&
                (MiiNWayBasePgAbleReg & NWAY_LPAR_PAUSE)) {

                if (OriginalFlowControl == FLOW_CONTROL_FULL) {
                    FlowControl = FLOW_CONTROL_FULL;
                    DEBUGOUT("Flow Control = FULL.\r\n");
                } else {
                    FlowControl = FLOW_CONTROL_RECEIVE_PAUSE;
                    DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
                }
            }

            else if (!(MiiNWayAdvertiseReg & NWAY_AR_PAUSE) &&
                     (MiiNWayAdvertiseReg & NWAY_AR_ASM_DIR) &&
                     (MiiNWayBasePgAbleReg & NWAY_LPAR_PAUSE) &&
                     (MiiNWayBasePgAbleReg & NWAY_LPAR_ASM_DIR)) {
                FlowControl = FLOW_CONTROL_TRANSMIT_PAUSE;
                DEBUGOUT("Flow Control = TX PAUSE frames only.\r\n");
            }

            else if ((MiiNWayAdvertiseReg & NWAY_AR_PAUSE) &&
                     (MiiNWayAdvertiseReg & NWAY_AR_ASM_DIR) &&
                     !(MiiNWayBasePgAbleReg & NWAY_LPAR_PAUSE) &&
                     (MiiNWayBasePgAbleReg & NWAY_LPAR_ASM_DIR)) {
                FlowControl = FLOW_CONTROL_RECEIVE_PAUSE;
                DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
            }

            else if (OriginalFlowControl == FLOW_CONTROL_NONE ||
                     OriginalFlowControl ==
                     FLOW_CONTROL_TRANSMIT_PAUSE) {
                FlowControl = FLOW_CONTROL_NONE;
                DEBUGOUT("Flow Control = NONE.\r\n");
            } else {
                FlowControl = FLOW_CONTROL_RECEIVE_PAUSE;
                DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
            }

            GetSpeedAndDuplex(&Speed, &Duplex);

            if (Duplex == HALF_DUPLEX)
                FlowControl = FLOW_CONTROL_NONE;

            ForceMacFlowControlSetting();
        } else {
            DEBUGOUT("Copper PHY and Auto Neg has not completed.\r\n");
        }
    }
}

VOID ForceMacFlowControlSetting(void)
 {
    UINT32 CtrlRegValue;

    DEBUGFUNC("ForceMacFlowControlSetting")

        CtrlRegValue = E1000_READ_REG(Ctrl);

    switch (FlowControl) {
    case FLOW_CONTROL_NONE:

        CtrlRegValue &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
        break;

    case FLOW_CONTROL_RECEIVE_PAUSE:

        CtrlRegValue &= (~E1000_CTRL_TFCE);
        CtrlRegValue |= E1000_CTRL_RFCE;
        break;

    case FLOW_CONTROL_TRANSMIT_PAUSE:

        CtrlRegValue &= (~E1000_CTRL_RFCE);
        CtrlRegValue |= E1000_CTRL_TFCE;
        break;

    case FLOW_CONTROL_FULL:

        CtrlRegValue |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
        break;

    default:

        DEBUGOUT("Flow control param set incorrectly\n");
        ASSERT(0);

        break;
    }

#if (MACTYPE == MAC_WISEMAN_2_0)
        CtrlRegValue &= (~E1000_CTRL_TFCE);
#endif
    E1000_WRITE_REG(Ctrl, CtrlRegValue);
}

VOID CheckForLink(void)
 {
    UINT32 RxcwRegValue;
    UINT32 CtrlRegValue;
    UINT32 StatusRegValue;
    UINT32 RctlRegValue;
    UINT16 PhyData;
    UINT16 LinkPartnerCapability;

    DEBUGFUNC("CheckForLink")

        CtrlRegValue = E1000_READ_REG(Ctrl);

    StatusRegValue = E1000_READ_REG(Status);

    RxcwRegValue = E1000_READ_REG(Rxcw);

    if (MediaType == MEDIA_TYPE_COPPER && GetLinkStatus) {

        PhyData = ReadPhyRegister(
                                  PHY_MII_STATUS_REG, PhyAddress);

        PhyData = ReadPhyRegister(
                                  PHY_MII_STATUS_REG, PhyAddress);

        if (PhyData & MII_SR_LINK_STATUS) {
            GetLinkStatus = FALSE;
        } else {
            DEBUGOUT("**** CFL - No link detected. ****\r\n");
            return;
        }

        if (!AutoNeg) {
            return;
        }

        switch (PhyId) {
        case PAXSON_PHY_88E1000:
        case PAXSON_PHY_88E1000S:
        case PAXSON_PHY_INTEGRATED:

#if (MACTYPE > MAC_WAINWRIGHT) 
                DEBUGOUT
                    ("CFL - Auto-Neg complete.  Configuring Collision Distance.");
                ConfigureCollisionDistance();
#else 

                PhyData = ReadPhyRegister(
                                          PXN_PHY_SPEC_STAT_REG,
                                          PhyAddress);

                DEBUGOUT1("CFL - Auto-Neg complete.  PhyData = %x\r\n",
                          PhyData);
                ConfigureMacToPhySettings( PhyData);
#endif

            ConfigFlowControlAfterLinkUp();
	    break;

        default:
            DEBUGOUT("CFL - Invalid PHY detected.\r\n");

        }

        if (TbiCompatibilityEnable) {
            LinkPartnerCapability = ReadPhyRegister(
                                                    PHY_AUTONEG_LP_BPA,
                                                    PhyAddress);
            if (LinkPartnerCapability & (NWAY_LPAR_10T_HD_CAPS |
                                         NWAY_LPAR_10T_FD_CAPS |
                                         NWAY_LPAR_100TX_HD_CAPS |
                                         NWAY_LPAR_100TX_FD_CAPS |
                                         NWAY_LPAR_100T4_CAPS)) {

                if (TbiCompatibilityOn) {

                    RctlRegValue = E1000_READ_REG(Rctl);
                    RctlRegValue &= ~E1000_RCTL_SBP;
                    E1000_WRITE_REG(Rctl, RctlRegValue);
                    TbiCompatibilityOn = FALSE;
                }
            } else {

                if (!TbiCompatibilityOn) {
                    TbiCompatibilityOn = TRUE;
                    RctlRegValue = E1000_READ_REG(Rctl);
                    RctlRegValue |= E1000_RCTL_SBP;
                    E1000_WRITE_REG(Rctl, RctlRegValue);
                }
            }
        }
    }

    else
        if ((MediaType == MEDIA_TYPE_FIBER) &&
            (!(StatusRegValue & E1000_STATUS_LU)) &&
            (!(CtrlRegValue & E1000_CTRL_SWDPIN1)) &&
            (!(RxcwRegValue & E1000_RXCW_C))) {
        if (AutoNegFailed == 0) {
            AutoNegFailed = 1;
            return;
        }

        DEBUGOUT("NOT RXing /C/, disable AutoNeg and force link.\r\n");

        E1000_WRITE_REG(Txcw, (TxcwRegValue & ~E1000_TXCW_ANE));

        CtrlRegValue = E1000_READ_REG(Ctrl);
        CtrlRegValue |= (E1000_CTRL_SLU | E1000_CTRL_FD);
        E1000_WRITE_REG(Ctrl, CtrlRegValue);

        ConfigFlowControlAfterLinkUp();
    }
        else if ((MediaType == MEDIA_TYPE_FIBER) &&
                 (CtrlRegValue & E1000_CTRL_SLU) &&
                 (RxcwRegValue & E1000_RXCW_C)) {

        DEBUGOUT("RXing /C/, enable AutoNeg and stop forcing link.\r\n");

        E1000_WRITE_REG(Txcw, TxcwRegValue);

        E1000_WRITE_REG(Ctrl, (CtrlRegValue & ~E1000_CTRL_SLU));
    }

    return;
}


VOID
GetSpeedAndDuplex(PUINT16 Speed, PUINT16 Duplex)
 {
    UINT32 DeviceStatusReg;

    DEBUGFUNC("GetSpeedAndDuplex")

//        if (AdapterStopped) {
//        *Speed = 0;
//        *Duplex = 0;
//        return;
//    }

#    if (MACTYPE >= MAC_LIVENGOOD) 
        DEBUGOUT("Livengood MAC\n");
        DeviceStatusReg = E1000_READ_REG(Status);
        if (DeviceStatusReg & E1000_STATUS_SPEED_1000) {
            *Speed = SPEED_1000;
            DEBUGOUT("   1000 Mbs\n");
        } else if (DeviceStatusReg & E1000_STATUS_SPEED_100) {
            *Speed = SPEED_100;
            DEBUGOUT("   100 Mbs\n");
        } else {
            *Speed = SPEED_10;
            DEBUGOUT("   10 Mbs\n");
        }

        if (DeviceStatusReg & E1000_STATUS_FD) {
            *Duplex = FULL_DUPLEX;
            DEBUGOUT("   Full Duplex\r\n");
        } else {
            *Duplex = HALF_DUPLEX;
            DEBUGOUT("   Half Duplex\r\n");
        }
#else  
        DEBUGOUT("Wiseman MAC - 1000 Mbs, Full Duplex\r\n");
        *Speed = SPEED_1000;
        *Duplex = FULL_DUPLEX;
#endif

    return;
}
#endif

UINT16 ReadEepromWord(UINT16 Reg)
 {
    UINT16 Data;

    ASSERT(Reg < EEPROM_WORD_SIZE);

    E1000_WRITE_REG(Eecd, E1000_EECS);

    ShiftOutBits(EEPROM_READ_OPCODE, 3);
    ShiftOutBits(Reg, 6);

    Data = ShiftInBits();

    EepromCleanup();
    return (Data);
}

STATIC
    VOID ShiftOutBits(UINT16 Data, UINT16 Count)
 {
    UINT32 EecdRegValue;
    UINT32 Mask;

    Mask = 0x01 << (Count - 1);

    EecdRegValue = E1000_READ_REG(Eecd);

    EecdRegValue &= ~(E1000_EEDO | E1000_EEDI);

    do {

        EecdRegValue &= ~E1000_EEDI;

        if (Data & Mask)
            EecdRegValue |= E1000_EEDI;

        E1000_WRITE_REG(Eecd, EecdRegValue);

        DelayInMicroseconds(50);

        RaiseClock(&EecdRegValue);
        LowerClock(&EecdRegValue);

        Mask = Mask >> 1;

    } while (Mask);

    EecdRegValue &= ~E1000_EEDI;

    E1000_WRITE_REG(Eecd, EecdRegValue);
}

STATIC VOID RaiseClock(UINT32 * EecdRegValue)
 {

    *EecdRegValue = *EecdRegValue | E1000_EESK;

    E1000_WRITE_REG(Eecd, *EecdRegValue);

    DelayInMicroseconds(50);
}

STATIC VOID LowerClock(UINT32 * EecdRegValue)
 {

    *EecdRegValue = *EecdRegValue & ~E1000_EESK;

    E1000_WRITE_REG(Eecd, *EecdRegValue);

    DelayInMicroseconds(50);
}

STATIC UINT16 ShiftInBits(void)
 {
    UINT32 EecdRegValue;
    UINT i;
    UINT16 Data;

    EecdRegValue = E1000_READ_REG(Eecd);

    EecdRegValue &= ~(E1000_EEDO | E1000_EEDI);
    Data = 0;

    for (i = 0; i < 16; i++) {
        Data = Data << 1;
        RaiseClock(&EecdRegValue);

        EecdRegValue = E1000_READ_REG(Eecd);

        EecdRegValue &= ~(E1000_EEDI);
        if (EecdRegValue & E1000_EEDO)
            Data |= 1;

        LowerClock(&EecdRegValue);
    }

    return (Data);
}

STATIC VOID EepromCleanup()
 {
    UINT32 EecdRegValue;

    EecdRegValue = E1000_READ_REG(Eecd);

    EecdRegValue &= ~(E1000_EECS | E1000_EEDI);

    E1000_WRITE_REG(Eecd, EecdRegValue);

    RaiseClock(&EecdRegValue);
    LowerClock(&EecdRegValue);
}
#ifdef EEPROM_CHECKSUM
BOOLEAN ValidateEepromChecksum(void)
 {
    UINT16 Checksum = 0;
    UINT16 Iteration;

    for (Iteration = 0; Iteration < (EEPROM_CHECKSUM_REG + 1); Iteration++)
        Checksum += ReadEepromWord(Iteration);

    if (Checksum == (UINT16) EEPROM_SUM)
        return (TRUE);
    else
        return (FALSE);
}

VOID UpdateEepromChecksum(void)
 {
    UINT16 Checksum = 0;
    UINT16 Iteration;

    for (Iteration = 0; Iteration < EEPROM_CHECKSUM_REG; Iteration++)
        Checksum += ReadEepromWord(Iteration);

    Checksum = (UINT16) EEPROM_SUM - Checksum;

    WriteEepromWord(EEPROM_CHECKSUM_REG, Checksum);
}

BOOLEAN WriteEepromWord(UINT16 Reg, UINT16 Data)
 {

    E1000_WRITE_REG(Eecd, E1000_EECS);

    StandBy();

    ShiftOutBits(EEPROM_EWEN_OPCODE, 5);
    ShiftOutBits(Reg, 4);

    StandBy();

    ShiftOutBits(EEPROM_ERASE_OPCODE, 3);
    ShiftOutBits(Reg, 6);

    if (!WaitEepromCommandDone()) {
        return (FALSE);
    }

    ShiftOutBits(EEPROM_EWDS_OPCODE, 5);
    ShiftOutBits(Reg, 4);

    StandBy();

    ShiftOutBits(EEPROM_EWEN_OPCODE, 5);
    ShiftOutBits(Reg, 4);

    StandBy();

    ShiftOutBits(EEPROM_WRITE_OPCODE, 3);
    ShiftOutBits(Reg, 6);
    ShiftOutBits(Data, 16);

    if (!WaitEepromCommandDone()) {
        return (FALSE);
    }

    ShiftOutBits(EEPROM_EWDS_OPCODE, 5);
    ShiftOutBits(Reg, 4);

    EepromCleanup();

    return (TRUE);
}

STATIC UINT16 WaitEepromCommandDone(void)
 {
    UINT32 EecdRegValue;
    UINT i;

    StandBy();

    for (i = 0; i < 200; i++) {
        EecdRegValue = E1000_READ_REG(Eecd);

        if (EecdRegValue & E1000_EEDO)
            return (TRUE);

        DelayInMicroseconds(5);
    }

    return (FALSE);
}

STATIC VOID StandBy(void)
 {
    UINT32 EecdRegValue;

    EecdRegValue = E1000_READ_REG(Eecd);

    EecdRegValue &= ~(E1000_EECS | E1000_EESK);

    E1000_WRITE_REG(Eecd, EecdRegValue);

    DelayInMicroseconds(5);

    EecdRegValue |= E1000_EECS;

    E1000_WRITE_REG(Eecd, EecdRegValue);
}
#endif


