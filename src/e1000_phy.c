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

/* Workfile: phy.c  */
/* Revision: 31  */
/* Date: 8/03/01 10:39a  */

#include "e1000.h"

#define GOOD_MII_IF 0

UINT16
ReadPhyRegister(IN UINT32 RegAddress, IN UINT32 PhyAddress);

VOID
WritePhyRegister(IN UINT32 RegAddress,
                 IN UINT32 PhyAddress, IN UINT16 Data);

STATIC
    VOID
MIIShiftOutPhyData(IN UINT32 Data, IN UINT16 Count);

STATIC
    VOID
RaiseMdcClock(IN OUT UINT32 * CtrlRegValue);

STATIC
    VOID
LowerMdcClock(IN OUT UINT32 * CtrlRegValue);

STATIC UINT16 MIIShiftInPhyData(void);

VOID PhyHardwareReset(void);

BOOLEAN PhyReset(void);

BOOLEAN PhySetup(IN UINT32 DeviceControlReg);

STATIC BOOLEAN PhySetupAutoNegAdvertisement(void);

STATIC VOID PhyForceSpeedAndDuplex(void);

VOID
ConfigureMacToPhySettings(IN UINT16 MiiRegisterData);

VOID ConfigureCollisionDistance(void);

VOID DisplayMiiContents(IN UINT8 PhyAddress);

UINT32 AutoDetectGigabitPhy(void);

VOID PxnPhyResetDsp(void);

VOID PxnIntegratedPhyLoopback(IN UINT16 Speed);

VOID PxnPhyEnableReceiver(void);

VOID PxnPhyDisableReceiver(void);

BOOLEAN WaitForAutoNeg(void);

UINT16
ReadPhyRegister(IN UINT32 RegAddress, IN UINT32 PhyAddress)
 {
    UINT32 i;
    UINT32 Data;
    UINT32 Command = 0;

    ASSERT(RegAddress <= MAX_PHY_REG_ADDRESS);

#if (MACTYPE > MAC_LIVENGOOD)

        Command = ((RegAddress << MDI_REGADD_SHIFT) |
                   (PhyAddress << MDI_PHYADD_SHIFT) | (E1000_MDI_READ));

        E1000_WRITE_REG(Mdic, Command);

        for (i = 0; i < 10; i++) {
            DelayInMicroseconds(10);

            Data = E1000_READ_REG(Mdic);

            if (Data & E1000_MDI_READY)
                break;
        }
#else

        MIIShiftOutPhyData(PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

        Command = ((RegAddress) |
                   (PhyAddress << 5) |
                   (PHY_OP_READ << 10) | (PHY_SOF << 12));

        MIIShiftOutPhyData(Command, 14);

        Data = (UINT32) MIIShiftInPhyData();
#endif

    ASSERT(!(Data & E1000_MDI_ERR));

    return ((UINT16) Data);
}

VOID
WritePhyRegister(IN UINT32 RegAddress,
                 IN UINT32 PhyAddress, IN UINT16 Data)
{
    UINT32 i;
    UINT32 Command = 0;
    UINT32 MdicRegValue;

    ASSERT(RegAddress <= MAX_PHY_REG_ADDRESS);

# if (MACTYPE > MAC_LIVENGOOD)

        Command = (((UINT32) Data) |
                   (RegAddress << MDI_REGADD_SHIFT) |
                   (PhyAddress << MDI_PHYADD_SHIFT) | (E1000_MDI_WRITE));

        E1000_WRITE_REG(Mdic, Command);

        for (i = 0; i < 10; i++) {
            DelayInMicroseconds(10);

            MdicRegValue = E1000_READ_REG(Mdic);

            if (MdicRegValue & E1000_MDI_READY)
                break;
        }

#else 

        MIIShiftOutPhyData(PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

        Command = ((PHY_TURNAROUND) |
                   (RegAddress << 2) |
                   (PhyAddress << 7) |
                   (PHY_OP_WRITE << 12) | (PHY_SOF << 14));
        Command <<= 16;
        Command |= ((UINT32) Data);

        MIIShiftOutPhyData(Command, 32);
#endif

    return;
}

#if MACTYPE <= MAC_LIVENGOOD

STATIC UINT16 MIIShiftInPhyData(void)
 {
    UINT32 CtrlRegValue;
    UINT16 Data = 0;
    UINT8 i;

    CtrlRegValue = E1000_READ_REG(Ctrl);

    CtrlRegValue &= ~E1000_CTRL_MDIO_DIR;
    CtrlRegValue &= ~E1000_CTRL_MDIO;

    E1000_WRITE_REG(Ctrl, CtrlRegValue);

    RaiseMdcClock(&CtrlRegValue);
    LowerMdcClock(&CtrlRegValue);

    for (Data = 0, i = 0; i < 16; i++) {
        Data = Data << 1;
        RaiseMdcClock(&CtrlRegValue);

        CtrlRegValue = E1000_READ_REG(Ctrl);

        if (CtrlRegValue & E1000_CTRL_MDIO)
            Data |= 1;

        LowerMdcClock(&CtrlRegValue);
    }

    RaiseMdcClock(&CtrlRegValue);
    LowerMdcClock(&CtrlRegValue);

    CtrlRegValue &= ~E1000_CTRL_MDIO;

    return (Data);
}

STATIC
    VOID
MIIShiftOutPhyData(IN UINT32 Data, IN UINT16 Count)
 {
    UINT32 CtrlRegValue;
    UINT32 Mask;

    if (Count > 32)
        ASSERT(0);

    Mask = 0x01;
    Mask <<= (Count - 1);

    CtrlRegValue = E1000_READ_REG(Ctrl);

    CtrlRegValue |= (E1000_CTRL_MDIO_DIR | E1000_CTRL_MDC_DIR);

    while (Mask) {

        if (Data & Mask)
            CtrlRegValue |= E1000_CTRL_MDIO;
        else
            CtrlRegValue &= ~E1000_CTRL_MDIO;

        E1000_WRITE_REG(Ctrl, CtrlRegValue);

        DelayInMicroseconds(2);

        RaiseMdcClock(&CtrlRegValue);
        LowerMdcClock(&CtrlRegValue);

        Mask = Mask >> 1;
    }

    CtrlRegValue &= ~E1000_CTRL_MDIO;
}

STATIC
    VOID
RaiseMdcClock(IN OUT UINT32 * CtrlRegValue)
 {

    E1000_WRITE_REG(Ctrl, (*CtrlRegValue | E1000_CTRL_MDC));

    DelayInMicroseconds(2);
}

STATIC
    VOID
LowerMdcClock(IN OUT UINT32 * CtrlRegValue)
 {

    E1000_WRITE_REG(Ctrl, (*CtrlRegValue & ~E1000_CTRL_MDC));

    DelayInMicroseconds(2);
}
#endif

VOID PhyHardwareReset()
 {
    UINT32 ExtCtrlRegValue, CtrlRegValue;

    DEBUGFUNC("PhyHardwareReset")

        DEBUGOUT("Resetting Phy...\n");

#    if (MACTYPE > MAC_LIVENGOOD) 

        CtrlRegValue = E1000_READ_REG(Ctrl);

        CtrlRegValue |= E1000_CTRL_PHY_RST;

        E1000_WRITE_REG(Ctrl, CtrlRegValue);

        DelayInMilliseconds(20);

        CtrlRegValue &= ~E1000_CTRL_PHY_RST;

        E1000_WRITE_REG(Ctrl, CtrlRegValue);

        DelayInMilliseconds(20);
#     else 

        ExtCtrlRegValue = E1000_READ_REG(Exct);

        ExtCtrlRegValue |= E1000_CTRL_PHY_RESET_DIR4;

        E1000_WRITE_REG(Exct, ExtCtrlRegValue);

        DelayInMilliseconds(20);

        ExtCtrlRegValue = E1000_READ_REG(Exct);

        ExtCtrlRegValue &= ~E1000_CTRL_PHY_RESET4;

        E1000_WRITE_REG(Exct, ExtCtrlRegValue);

        DelayInMilliseconds(20);

        ExtCtrlRegValue = E1000_READ_REG(Exct);

        ExtCtrlRegValue |= E1000_CTRL_PHY_RESET4;

        E1000_WRITE_REG(Exct, ExtCtrlRegValue);

        DelayInMilliseconds(20);
#endif

    return;
}
#if 0
BOOLEAN PhyReset(void)
 {
    UINT16 RegData;
    UINT16 i;

    DEBUGFUNC("PhyReset")

        RegData = ReadPhyRegister(PHY_MII_CTRL_REG, PhyAddress);

    RegData |= MII_CR_RESET;

    WritePhyRegister(PHY_MII_CTRL_REG, PhyAddress, RegData);

    i = 0;
    while ((RegData & MII_CR_RESET) && i++ < 500) {
        RegData = ReadPhyRegister(PHY_MII_CTRL_REG, PhyAddress);
        DelayInMicroseconds(1);
    }

    if (i >= 500) {
        DEBUGOUT("Timeout waiting for PHY to reset.\n");
        return FALSE;
    }

    return TRUE;
}
#endif

BOOLEAN PhySetup(UINT32 DeviceControlReg)
 {
    UINT32 DeviceCtrlReg = 0;
    UINT16 MiiCtrlReg, MiiStatusReg;
    UINT16 MiiAutoNegAdvertiseReg, Mii1000TCtrlReg;
    UINT16 i, Data;
    UINT16 AutoNegHwSetting;
    UINT16 AutoNegFCSetting;
    BOOLEAN RestartAutoNeg = FALSE;
    BOOLEAN ForceAutoNegRestart = FALSE;
    DEBUGFUNC("PhySetup")

        ASSERT(MacType >= MAC_LIVENGOOD);

    if (MacType > MAC_WAINWRIGHT) {
        DeviceControlReg |= (E1000_CTRL_ASDE | E1000_CTRL_SLU);
        E1000_WRITE_REG(Ctrl, DeviceControlReg);
    } else {
        DeviceControlReg |= (E1000_CTRL_FRCSPD |
                             E1000_CTRL_FRCDPX | E1000_CTRL_SLU);
        E1000_WRITE_REG(Ctrl, DeviceControlReg);

        if (MacType == MAC_LIVENGOOD)
            PhyHardwareReset();
    }

    PhyAddress = AutoDetectGigabitPhy();

    if (PhyAddress > MAX_PHY_REG_ADDRESS) {

        DEBUGOUT("PhySetup failure, did not detect valid phy.\n");
        return (FALSE);
    }

    DEBUGOUT1("Phy ID = %x \n", PhyId);

    MiiCtrlReg = ReadPhyRegister(PHY_MII_CTRL_REG, PhyAddress);

    DEBUGOUT1("MII Ctrl Reg contents = %x\n", MiiCtrlReg);

    if (!(MiiCtrlReg & MII_CR_AUTO_NEG_EN))
        ForceAutoNegRestart = TRUE;

    MiiCtrlReg &= ~(MII_CR_ISOLATE);

    WritePhyRegister(PHY_MII_CTRL_REG, PhyAddress, MiiCtrlReg);

    Data = ReadPhyRegister(PXN_PHY_SPEC_CTRL_REG, PhyAddress);

    Data |= PXN_PSCR_ASSERT_CRS_ON_TX;

    DEBUGOUT1("Paxson PSCR: %x \n", Data);

    WritePhyRegister( PXN_PHY_SPEC_CTRL_REG, PhyAddress, Data);

    Data = ReadPhyRegister(PXN_EXT_PHY_SPEC_CTRL_REG, PhyAddress);

    Data |= PXN_EPSCR_TX_CLK_25;

    WritePhyRegister(PXN_EXT_PHY_SPEC_CTRL_REG, PhyAddress, Data);

    MiiAutoNegAdvertiseReg = ReadPhyRegister(PHY_AUTONEG_ADVERTISEMENT,
                                             PhyAddress);

    AutoNegHwSetting = (MiiAutoNegAdvertiseReg >> 5) & 0xF;

    Mii1000TCtrlReg = ReadPhyRegister(PHY_1000T_CTRL_REG,
                                      PhyAddress);

    AutoNegHwSetting |= ((Mii1000TCtrlReg & 0x0300) >> 4);

    AutoNegFCSetting = ((MiiAutoNegAdvertiseReg & 0x0C00) >> 10);

    AutoNegAdvertised &= AUTONEG_ADVERTISE_SPEED_DEFAULT;

    if (AutoNegAdvertised == 0)
        AutoNegAdvertised = AUTONEG_ADVERTISE_SPEED_DEFAULT;

    if (!ForceAutoNegRestart && AutoNeg &&
        (AutoNegAdvertised == AutoNegHwSetting) &&
        (FlowControl == AutoNegFCSetting)) {
        DEBUGOUT("No overrides - Reading MII Status Reg..\n");

        MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                       PhyAddress);

        MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                       PhyAddress);

        DEBUGOUT1("MII Status Reg contents = %x\n", MiiStatusReg);

        if (MiiStatusReg & MII_SR_LINK_STATUS) {
            Data = ReadPhyRegister(PXN_PHY_SPEC_STAT_REG,
                                   PhyAddress);
            DEBUGOUT1("Paxson Phy Specific Status Reg contents = %x\n",
                      Data);

#if (MACTYPE > MAC_WAINWRIGHT)
                ConfigureCollisionDistance();
#else
                ConfigureMacToPhySettings(Data);
#endif
#ifdef USE_COMPLEX_VERSION
            ConfigFlowControlAfterLinkUp();
#endif
            return (TRUE);
        }
    }

    if (AutoNeg) {
        DEBUGOUT
            ("Livengood - Reconfiguring auto-neg advertisement params\n");
        RestartAutoNeg = PhySetupAutoNegAdvertisement();
    } else {
        DEBUGOUT("Livengood - Forcing speed and duplex\n");
        // PhyForceSpeedAndDuplex();
    }

    if (RestartAutoNeg) {
        DEBUGOUT("Restarting Auto-Neg\n");
	
        MiiCtrlReg = ReadPhyRegister(PHY_MII_CTRL_REG,
                                     PhyAddress);

        MiiCtrlReg |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);

        WritePhyRegister(PHY_MII_CTRL_REG,
                         PhyAddress, MiiCtrlReg);

        if (WaitAutoNegComplete)
            WaitForAutoNeg();

    }

    MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                   PhyAddress);

    MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                   PhyAddress);

    DEBUGOUT1("Checking for link status - MII Status Reg contents = %x\n",
              MiiStatusReg);

    for (i = 0; i < 10; i++) {
        if (MiiStatusReg & MII_SR_LINK_STATUS) {
            break;
        }
        DelayInMicroseconds(10);
        DEBUGOUT(". ");

        MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                       PhyAddress);

        MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                       PhyAddress);
    }

    if (MiiStatusReg & MII_SR_LINK_STATUS) {

        Data = ReadPhyRegister(PXN_PHY_SPEC_STAT_REG, PhyAddress);

        DEBUGOUT1("Paxson Phy Specific Status Reg contents = %x\n", Data);

#        if (MACTYPE > MAC_WAINWRIGHT)
            ConfigureCollisionDistance();
#        else
            ConfigureMacToPhySettings(Data);
#endif

#ifdef USE_COMPLEX_VERSION
        ConfigFlowControlAfterLinkUp();
#endif
        DEBUGOUT("Valid link established!!!\n");
    } else {
        DEBUGOUT("Unable to establish link!!!\n");
    }

    return (TRUE);
}

BOOLEAN PhySetupAutoNegAdvertisement(void)
 {

#ifdef USE_COMPLEX_VERSION
    UINT16 MiiAutoNegAdvertiseReg, Mii1000TCtrlReg;

    DEBUGFUNC("PhySetupAutoNegAdvertisement")

        MiiAutoNegAdvertiseReg = ReadPhyRegister(PHY_AUTONEG_ADVERTISEMENT,
                                                 PhyAddress);

    Mii1000TCtrlReg = ReadPhyRegister(PHY_1000T_CTRL_REG,
                                      PhyAddress);

    MiiAutoNegAdvertiseReg &= ~REG4_SPEED_MASK;
    Mii1000TCtrlReg &= ~REG9_SPEED_MASK;

    DEBUGOUT2("AutoNegAdvertised %x %x\n", AutoNegAdvertised,PhyAddress);

    if (AutoNegAdvertised & ADVERTISE_10_HALF) {
        DEBUGOUT("Advertise 10mb Half duplex\n");
        MiiAutoNegAdvertiseReg |= NWAY_AR_10T_HD_CAPS;
    }

    if (AutoNegAdvertised & ADVERTISE_10_FULL) {
        DEBUGOUT("Advertise 10mb Full duplex\n");
        MiiAutoNegAdvertiseReg |= NWAY_AR_10T_FD_CAPS;
    }

    if (AutoNegAdvertised & ADVERTISE_100_HALF) {
        DEBUGOUT("Advertise 100mb Half duplex\n");
        MiiAutoNegAdvertiseReg |= NWAY_AR_100TX_HD_CAPS;
    }

    if (AutoNegAdvertised & ADVERTISE_100_FULL) {
        DEBUGOUT("Advertise 100mb Full duplex\n");
        MiiAutoNegAdvertiseReg |= NWAY_AR_100TX_FD_CAPS;
    }

    if (AutoNegAdvertised & ADVERTISE_1000_HALF) {
        DEBUGOUT
            ("Advertise 1000mb Half duplex requested, request denied!\n");
    }

    if (AutoNegAdvertised & ADVERTISE_1000_FULL) {
        DEBUGOUT("Advertise 1000mb Full duplex\n");
        Mii1000TCtrlReg |= CR_1000T_FD_CAPS;
    }

    switch (FlowControl) {
    case FLOW_CONTROL_NONE:

        MiiAutoNegAdvertiseReg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);

        break;

    case FLOW_CONTROL_RECEIVE_PAUSE:

        MiiAutoNegAdvertiseReg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);

        break;

    case FLOW_CONTROL_TRANSMIT_PAUSE:

        MiiAutoNegAdvertiseReg |= NWAY_AR_ASM_DIR;
        MiiAutoNegAdvertiseReg &= ~NWAY_AR_PAUSE;

        break;

    case FLOW_CONTROL_FULL:

        MiiAutoNegAdvertiseReg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);

        break;

    default:

        DEBUGOUT("Flow control param set incorrectly\n");
        ASSERT(0);
        break;
    }

    WritePhyRegister(PHY_AUTONEG_ADVERTISEMENT,
                     PhyAddress, MiiAutoNegAdvertiseReg);

    DEBUGOUT1("Auto-Neg Advertising %x\n", MiiAutoNegAdvertiseReg);

    WritePhyRegister(PHY_1000T_CTRL_REG,
                     PhyAddress, Mii1000TCtrlReg);
#else
    UINT16 Mii1000TCtrlReg;
    Mii1000TCtrlReg = ReadPhyRegister(PHY_1000T_CTRL_REG,
                                      PhyAddress);
    Mii1000TCtrlReg &= ~REG9_SPEED_MASK;
    Mii1000TCtrlReg |= CR_1000T_FD_CAPS;
    WritePhyRegister(PHY_AUTONEG_ADVERTISEMENT,
                     PhyAddress, 0x1e1);
#endif
    return (TRUE);
}
#if 0
STATIC VOID PhyForceSpeedAndDuplex(void)
 {
    UINT16 MiiCtrlReg;
    UINT16 MiiStatusReg;
    UINT16 PhyData;
    UINT16 i;
    UINT32 TctlReg;
    UINT32 DeviceCtrlReg;
    UINT32 Shift32;

    DEBUGFUNC("PhyForceSpeedAndDuplex")

        FlowControl = FLOW_CONTROL_NONE;

    DEBUGOUT1("FlowControl = %d\n", FlowControl);

    DeviceCtrlReg = E1000_READ_REG(Ctrl);

    DeviceCtrlReg |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
    DeviceCtrlReg &= ~(DEVICE_SPEED_MASK);

    DeviceCtrlReg &= ~E1000_CTRL_ASDE;

    MiiCtrlReg = ReadPhyRegister(PHY_MII_CTRL_REG, PhyAddress);

    MiiCtrlReg &= ~MII_CR_AUTO_NEG_EN;

    if (Adapter->ForcedSpeedDuplex == FULL_100 ||
        Adapter->ForcedSpeedDuplex == FULL_10) {

        DeviceCtrlReg |= E1000_CTRL_FD;
        MiiCtrlReg |= MII_CR_FULL_DUPLEX;

        DEBUGOUT("Full Duplex\n");
    } else {

        DeviceCtrlReg &= ~E1000_CTRL_FD;
        MiiCtrlReg &= ~MII_CR_FULL_DUPLEX;

        DEBUGOUT("Half Duplex\n");
    }

    if (Adapter->ForcedSpeedDuplex == FULL_100 ||
        Adapter->ForcedSpeedDuplex == HALF_100) {

        DeviceCtrlReg |= E1000_CTRL_SPD_100;
        MiiCtrlReg |= MII_CR_SPEED_100;
        MiiCtrlReg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);

        DEBUGOUT("Forcing 100mb ");
    } else {

        DeviceCtrlReg &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
        MiiCtrlReg |= MII_CR_SPEED_10;
        MiiCtrlReg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);

        DEBUGOUT("Forcing 10mb ");
    }

    TctlReg = E1000_READ_REG(Tctl);
    DEBUGOUT1("TctlReg = %x\n", TctlReg);

    if (!(MiiCtrlReg & MII_CR_FULL_DUPLEX)) {

        TctlReg &= ~E1000_TCTL_COLD;
        Shift32 = E1000_HDX_COLLISION_DISTANCE;
        Shift32 <<= E1000_COLD_SHIFT;
        TctlReg |= Shift32;
    } else {

        TctlReg &= ~E1000_TCTL_COLD;
        Shift32 = E1000_FDX_COLLISION_DISTANCE;
        Shift32 <<= E1000_COLD_SHIFT;
        TctlReg |= Shift32;
    }

    E1000_WRITE_REG(Tctl, TctlReg);

    E1000_WRITE_REG(Ctrl, DeviceCtrlReg);

    PhyData = ReadPhyRegister(PXN_PHY_SPEC_CTRL_REG, PhyAddress);

    PhyData &= ~PXN_PSCR_AUTO_X_MODE;

    WritePhyRegister(PXN_PHY_SPEC_CTRL_REG, PhyAddress, PhyData);

    DEBUGOUT1("Paxson PSCR: %x \n", PhyData);

    MiiCtrlReg |= MII_CR_RESET;

    WritePhyRegister(PHY_MII_CTRL_REG, PhyAddress, MiiCtrlReg);

    if (Adapter->WaitAutoNegComplete) {

        DEBUGOUT("Waiting for forced speed/duplex link.\n");
        MiiStatusReg = 0;

#define PHY_WAIT_FOR_FORCED_TIME    20

        for (i = 20; i > 0; i--) {

            MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                           PhyAddress);

            MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                           PhyAddress);

            if (MiiStatusReg & MII_SR_LINK_STATUS) {
                break;
            }
            DelayInMilliseconds(100);
        }

        if (i == 0) {

            PxnPhyResetDsp();
        }

        for (i = 20; i > 0; i--) {
            if (MiiStatusReg & MII_SR_LINK_STATUS) {
                break;
            }

            DelayInMilliseconds(100);

            MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                           PhyAddress);

            MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                           PhyAddress);

        }
    }

    PhyData = ReadPhyRegister(PXN_EXT_PHY_SPEC_CTRL_REG,
                              PhyAddress);

    PhyData |= PXN_EPSCR_TX_CLK_25;

    WritePhyRegister(PXN_EXT_PHY_SPEC_CTRL_REG,
                     PhyAddress, PhyData);

    PhyData = ReadPhyRegister(PXN_PHY_SPEC_CTRL_REG, PhyAddress);

    PhyData |= PXN_PSCR_ASSERT_CRS_ON_TX;

    WritePhyRegister(PXN_PHY_SPEC_CTRL_REG, PhyAddress, PhyData);
    DEBUGOUT1("After force, Paxson Phy Specific Ctrl Reg = %4x\r\n",
              PhyData);

    return;
}
#endif

VOID
ConfigureMacToPhySettings(IN UINT16 MiiRegisterData)
 {
    UINT32 DeviceCtrlReg, TctlReg;
    UINT32 Shift32;

    DEBUGFUNC("ConfigureMacToPhySettings")

        TctlReg = E1000_READ_REG(Tctl);
    DEBUGOUT1("TctlReg = %x\n", TctlReg);

    DeviceCtrlReg = E1000_READ_REG(Ctrl);

    DeviceCtrlReg |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
    DeviceCtrlReg &= ~(DEVICE_SPEED_MASK);

    DEBUGOUT1("MII Register Data = %x\r\n", MiiRegisterData);

    DeviceCtrlReg &= ~E1000_CTRL_ILOS;

    if (MiiRegisterData & PXN_PSSR_DPLX) {
        DeviceCtrlReg |= E1000_CTRL_FD;

        TctlReg &= ~E1000_TCTL_COLD;
        Shift32 = E1000_FDX_COLLISION_DISTANCE;
        Shift32 <<= E1000_COLD_SHIFT;
        TctlReg |= Shift32;
    } else {
        DeviceCtrlReg &= ~E1000_CTRL_FD;

        if ((MiiRegisterData & PXN_PSSR_SPEED) == PXN_PSSR_1000MBS) {
            TctlReg &= ~E1000_TCTL_COLD;
            Shift32 = E1000_GB_HDX_COLLISION_DISTANCE;
            Shift32 <<= E1000_COLD_SHIFT;
            TctlReg |= Shift32;

            TctlReg |= E1000_TCTL_PBE;

        } else {
            TctlReg &= ~E1000_TCTL_COLD;
            Shift32 = E1000_HDX_COLLISION_DISTANCE;
            Shift32 <<= E1000_COLD_SHIFT;
            TctlReg |= Shift32;
        }
    }

    if ((MiiRegisterData & PXN_PSSR_SPEED) == PXN_PSSR_1000MBS)
        DeviceCtrlReg |= E1000_CTRL_SPD_1000;
    else if ((MiiRegisterData & PXN_PSSR_SPEED) == PXN_PSSR_100MBS)
        DeviceCtrlReg |= E1000_CTRL_SPD_100;
    else
        DeviceCtrlReg &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);

    E1000_WRITE_REG(Tctl, TctlReg);

    E1000_WRITE_REG(Ctrl, DeviceCtrlReg);

    return;
}

VOID ConfigureCollisionDistance(void)
 {

#ifdef USE_COMPLEX_VERSION
    UINT32 TctlReg;
    UINT16 Speed;
    UINT16 Duplex;
    UINT32 Shift32;

    DEBUGFUNC("ConfigureCollisionDistance")

        GetSpeedAndDuplex(&Speed, &Duplex);

    TctlReg = E1000_READ_REG(Tctl);
    DEBUGOUT1("TctlReg = %x\n", TctlReg);

    TctlReg &= ~E1000_TCTL_COLD;

    if (Duplex == FULL_DUPLEX) {

        Shift32 = E1000_FDX_COLLISION_DISTANCE;
        Shift32 <<= E1000_COLD_SHIFT;
        TctlReg |= Shift32;
    } else {

        if (Speed == SPEED_1000) {
            Shift32 = E1000_GB_HDX_COLLISION_DISTANCE;
            Shift32 <<= E1000_COLD_SHIFT;
            TctlReg |= Shift32;

            TctlReg |= E1000_TCTL_PBE;

        } else {
            Shift32 = E1000_HDX_COLLISION_DISTANCE;
            Shift32 <<= E1000_COLD_SHIFT;
            TctlReg |= Shift32;
        }
    }

    E1000_WRITE_REG(Tctl, TctlReg);
#endif

    return;
}


UINT32 AutoDetectGigabitPhy(void)
{
    UINT32 PhyAddress = 1;
    UINT32 PhyIDHi;
    UINT16 PhyIDLo;
    BOOLEAN GotOne = FALSE;

    DEBUGFUNC("AutoDetectGigabitPhy")

        while ((!GotOne) && (PhyAddress <= MAX_PHY_REG_ADDRESS)) {

        PhyIDHi = ReadPhyRegister(PHY_PHY_ID_REG1, PhyAddress);

        DelayInMicroseconds(2);

        PhyIDLo = ReadPhyRegister(PHY_PHY_ID_REG2, PhyAddress);

        PhyId = (PhyIDLo | (PhyIDHi << 16)) & PHY_REVISION_MASK;

        if (PhyId == PAXSON_PHY_88E1000 ||
            PhyId == PAXSON_PHY_88E1000S ||
            PhyId == PAXSON_PHY_INTEGRATED) {
            DEBUGOUT2("PhyId 0x%x detected at address 0x%x\n",
                      PhyId, PhyAddress);

            GotOne = TRUE;
        } else {
            PhyAddress++;
        }

    }

    if (PhyAddress > MAX_PHY_REG_ADDRESS) {
        DEBUGOUT("Could not auto-detect Phy!\n");
    }

    return (PhyAddress);
}

VOID PxnPhyResetDsp(void)
{
    WritePhyRegister( 29, PhyAddress, 0x1d);
    WritePhyRegister( 30, PhyAddress, 0xc1);
    WritePhyRegister( 30, PhyAddress, 0x00);
}

BOOLEAN WaitForAutoNeg(void)
{
    BOOLEAN AutoNegComplete = FALSE;
    UINT16 i;
    UINT16 MiiStatusReg;

    DEBUGFUNC("WaitForAutoNeg");

    DEBUGOUT("Waiting for Auto-Neg to complete.\n");
    MiiStatusReg = 0;

    for (i = PHY_AUTO_NEG_TIME; i > 0; i--) {

        MiiStatusReg = ReadPhyRegister(PHY_MII_STATUS_REG,
                                       PhyAddress);

        if (MiiStatusReg & MII_SR_AUTONEG_COMPLETE) {
            AutoNegComplete = TRUE;
            break;
        }

        DelayInMilliseconds(100);
    }

    return (AutoNegComplete);
}
