#ifndef _BCMAUXSPI_HW_H_
#define _BCMAUXSPI_HW_H_
//
// Copyright (C) Microsoft.  All rights reserved.
//
//
// Module Name:
//
//   bcmauxspi-hw.h
//
// Abstract:
//
//   BCM AUX SPI Hardware Register Definitions
//

#pragma warning(disable:4201)   // nameless struct/union
#include <pshpack1.h>

//
// AUXSPI controller registers.
//

union BCM_AUX_IRQ_REG {
    ULONG AsUlong;
    struct {
        // LSB
        ULONG MiniUartIrq : 1;
        ULONG Spi1Irq : 1;
        ULONG Spi2Irq : 1;
        ULONG reserved : 29;
        // MSB
    };
};

union BCM_AUX_ENABLES_REG {
    ULONG AsUlong;
    struct {
        // LSB
        ULONG MiniUartEnable : 1;
        ULONG Spi1Enable : 1;
        ULONG Spi2Enable : 1;
        ULONG reserved : 29;
        // MSB
    };
};

union BCM_AUXSPI_CNTL0_REG {
    ULONG AsUlong;
    struct {
        // LSB
        ULONG ShiftLength : 6;
        ULONG ShiftOutMsbFirst : 1;
        ULONG InvertSpiClk : 1;
        ULONG OutRising : 1;
        ULONG ClearFifos : 1;
        ULONG InRising : 1;
        ULONG Enable : 1;
        ULONG DoutHoldTime : 2;
        ULONG VariableWidth : 1;
        ULONG VariableCs : 1;
        ULONG PostInputMode : 1;
        ULONG ChipSelects : 3;
        ULONG Speed : 12;
        // MSB
    };
};

union BCM_AUXSPI_CNTL1_REG {
    ULONG AsUlong;
    struct {
        // LSB
        ULONG KeepInput : 1;
        ULONG ShiftInMsbFirst : 1;
        ULONG reserved1 : 4;
        ULONG DoneIrq : 1;
        ULONG TxEmptyIrq : 1;
        ULONG CsHighTime : 3;
        ULONG reserved2 : 14;
        // MSB
    };
};

union BCM_AUXSPI_STAT_REG {
    ULONG AsUlong;
    struct {
        // LSB
        ULONG BitCount : 6;
        ULONG Busy : 1;
        ULONG RxEmpty : 1;
        ULONG TxEmpty : 1;
        ULONG TxFull  : 1;
        ULONG reserved1 : 22;
        // MSB
    };
};

union BCM_AUXSPI_IO_REG {
    ULONG AsUlong;
    struct {
        // LSB
        ULONG Data : 24;
        ULONG Width : 5;
        ULONG CsPattern : 3;
        // MSB
    };
};

struct BCM_AUXMU_REGISTERS {
    ULONG IoReg;
    ULONG IerReg;
    ULONG IirReg;
    ULONG LcrReg;
    ULONG McrReg;
    ULONG LsrReg;
    ULONG MsrReg;
    ULONG Scratch;
    ULONG CntlReg;
    ULONG StatReg;
    ULONG BaudReg;
};

struct BCM_AUXSPI_REGISTERS {
    ULONG Cntl0Reg;
    ULONG Cntl1Reg;
    ULONG StatReg;
    ULONG PeekReg;
    ULONG reserved1[4];
    ULONG IoReg;
    ULONG reserved2[3];
    ULONG TxHoldReg;
    ULONG reserved3[3];
};

struct BCM_AUX_REGISTERS {
    ULONG Irq;
    ULONG Enables;
    ULONG reserved1[14];
    BCM_AUXMU_REGISTERS MiniUart;
    ULONG reserved2[5];
    BCM_AUXSPI_REGISTERS Spi1;
    BCM_AUXSPI_REGISTERS Spi2;
};

static_assert(
    FIELD_OFFSET(BCM_AUX_REGISTERS, MiniUart) == 0x40,
    "Verifying offset of MiniUart registers");

static_assert(
    FIELD_OFFSET(BCM_AUX_REGISTERS, Spi1) == 0x80,
    "Verifying offset of Spi0 registers");

static_assert(
    FIELD_OFFSET(BCM_AUX_REGISTERS, Spi2) == 0xC0,
    "Verifying offset of Spi0 registers");

enum : ULONG {
    BCM_DEFAULT_SYSTEM_CLOCK_FREQ = 250000000,    // 250MHz
    BCM_AUXSPI_FIFO_DEPTH = 4,
};

#include <poppack.h> // pshpack1
#pragma warning(disable:4201)   // nameless struct/union

#endif // _BCMAUXSPI_HW_H_
