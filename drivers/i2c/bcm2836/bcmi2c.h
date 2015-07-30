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
// BCM : Broadcom Serial Controller
//

typedef struct BCMI2C_REGISTERS
{
    // using the chatty register names because
    // the short register names are not self explanatory
    __declspec(align(4)) ULONG Control;             // C
    __declspec(align(4)) ULONG Status;              // S
    __declspec(align(4)) ULONG DataLength;          // DLEN
    __declspec(align(4)) ULONG SlaveAddress;        // A
    __declspec(align(4)) ULONG DataFIFO;            // FIFO
    __declspec(align(4)) ULONG ClockDivider;        // DIV
    __declspec(align(4)) ULONG DataDelay;           // DEL
    __declspec(align(4)) ULONG ClockStretchTimeout; // CLKT
}
BCM_I2C_REGISTERS, *PBCM_I2C_REGISTERS;

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

//
// I2C.DLEN DataLength Register bit fields
//
#define BCM_I2C_REG_DLEN_MASK               0x0000FFFF

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
#define BCM_I2C_REG_DIV_CDIV                0x0000FFFE

//
// I2C.DEL DataDelay Register bit fields
//
#define BCM_I2C_REG_DEL_FEDL                0xFFFF0000
#define BCM_I2C_REG_DEL_REDL                0x0000FFFF

//
// I2C.CLKT ClockStretchTimeout Register bit fields
//
#define BCM_I2C_REG_CLKT_TOUT               0x0000FFFF

//
// Default values for Control Register
//
#define BCM_I2C_REG_CONTROL_DEFAULT BCM_I2C_REG_CONTROL_I2CEN
#define BCM_I2C_REG_TOUT_DEFAULT    0x40        
#define BCM_I2C_REG_DEL_DEFAULT     0x300030    

//
// I2C ClockDivider
//
#define BCM_I2C_CORE_CLOCK                  250000000L // 250MHz
#define BCM_I2C_CLOCK_RATE_LOWEST           ((BCM_I2C_CORE_CLOCK / BCM_I2C_REG_DIV_CDIV) + 1) 
                                                       // min. supported I2C bus clock rate
#define BCM_I2C_CLOCK_RATE_STANDARD         100000     // standard I2C bus clock rate
#define BCM_I2C_CLOCK_RATE_FAST             400000     // fast I2C bus clock rate

inline ULONG
BCMI2CSetClkDivider(ULONG clock)
{
    ULONG cdiv;
    if (clock < BCM_I2C_CLOCK_RATE_LOWEST)
    {
        clock = BCM_I2C_CLOCK_RATE_LOWEST;
    }
    else if (clock >= BCM_I2C_CLOCK_RATE_FAST)
    {
        clock = BCM_I2C_CLOCK_RATE_FAST;
    }
    
    cdiv = BCM_I2C_CORE_CLOCK / clock;

    return cdiv;
}

#define BCM_I2C_MAX_TRANSFER_LENGTH         BCM_I2C_REG_DLEN_MASK
#define BCM_I2C_MAX_BYTES_PER_TRANSFER      16
#define BCM_TA_BIT_TIMEOUT                  1000    // 1000us

#endif
