/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    bcmi2c.h

Abstract:

    This module contains the controller-specific type
    definitions for the BCM2841 I2C controller driver hardware.

Environment:

    kernel-mode only

Revision History:

--*/

//
// Includes for hardware register definitions.
//

#ifndef _BCMI2C_H_
#define _BCMI2C_H_

//
// BCM I2C controller registers.
// BSC : Broadcom Serial Controller
//

#include <pshpack1.h>
struct BCM_I2C_REGISTERS {
    ULONG Control;             // C
    ULONG Status;              // S
    ULONG DataLength;          // DLEN
    ULONG SlaveAddress;        // A
    ULONG DataFIFO;            // FIFO
    ULONG ClockDivider;        // DIV
    ULONG DataDelay;           // DEL
    ULONG ClockStretchTimeout; // CLKT
};
#include <poppack.h>

//
// I2C.C Control Register bit fields
//
#define BCM_I2C_REG_CONTROL_I2CEN           0x00008000
#define BCM_I2C_REG_CONTROL_INTR            0x00000400
#define BCM_I2C_REG_CONTROL_INTT            0x00000200
#define BCM_I2C_REG_CONTROL_INTD            0x00000100
#define BCM_I2C_REG_CONTROL_ST              0x00000080
#define BCM_I2C_REG_CONTROL_CLEAR           0x00000030
#define BCM_I2C_REG_CONTROL_READ            0x00000001

//
// I2C.S Status Register bit fields
//
#define BCM_I2C_REG_STATUS_CLKT             0x00000200
#define BCM_I2C_REG_STATUS_ERR              0x00000100
#define BCM_I2C_REG_STATUS_RXF              0x00000080
#define BCM_I2C_REG_STATUS_TXE              0x00000040
#define BCM_I2C_REG_STATUS_RXD              0x00000020
#define BCM_I2C_REG_STATUS_TXD              0x00000010
#define BCM_I2C_REG_STATUS_RXR              0x00000008
#define BCM_I2C_REG_STATUS_TXW              0x00000004
#define BCM_I2C_REG_STATUS_DONE             0x00000002
#define BCM_I2C_REG_STATUS_TA               0x00000001
#define BCM_I2C_REG_STATUS_MASK             0x000003FF

//
// I2C.DLEN DataLength Register bit fields
//
#define BCM_I2C_REG_DLEN_MASK               0x0000FFFF
#define BCM_I2C_MAX_TRANSFER_LENGTH         BCM_I2C_REG_DLEN_MASK

//
// I2C.A Address Register bit fields
//
#define BCM_I2C_REG_ADDRESS_MASK            0x0000007F

//
// I2C.FIFO DataFIFO Register bit fields
//
#define BCM_I2C_REG_FIFO_MASK               0x000000FF

//
// I2C.DIV ClockDivider Register bit fields
//
#define BCM_I2C_REG_CDIV_MASK               0x0000FFFE

//
// I2C.DEL DataDelay Register bit field scaled for 250Mhz clock operation.
//
#define BCM_I2C_REG_DEL_DEFAULT             0x00300030
#define BCM_I2C_REG_DEL_FEDL                0x50

//
// I2C.CLKT ClockStretchTimeout Register bit fields
//
#define BCM_I2C_REG_CLKT_TOUT_MASK          0x0000FFFF
#define BCM_I2C_REG_CLKT_TOUT_DEFAULT       BCM_I2C_REG_CLKT_TOUT_MASK

//
// I2C ClockDivider
//
#define BCM_I2C_CORE_CLOCK                  250000000   // 250MHz
#define BCM_I2C_MIN_CONNECTION_SPEED        (BCM_I2C_CORE_CLOCK / BCM_I2C_REG_CDIV_MASK)
#define BCM_I2C_MAX_CONNECTION_SPEED        400000      // highest tested speed
#define BCM_I2C_REG_CDIV_DEFAULT            ((BCM_I2C_CORE_CLOCK / 100000) & BCM_I2C_REG_CDIV_MASK)



#endif // _BCMI2C_H_
