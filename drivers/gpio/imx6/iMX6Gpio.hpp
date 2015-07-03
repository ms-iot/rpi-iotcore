#ifndef _IMX6GPIO_HPP_
#define _IMX6GPIO_HPP_ 1

//
// Freescale i.MX6 GPIO Client Driver
//
// Copyright(c) Microsoft Corporation
//
// All rights reserved.
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this software and associated documentation files(the ""Software""),
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and / or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
//
// Module Name:
//
//  iMX6Gpio.hpp
//
// Abstract:
//
//    This file contains types and definitions for i.MX6 Series GPIO
//    controller driver.
//
// Environment:
//
//    Kernel mode only.
//


// Signals in i.MX datasheets follow the pattern GPIO<bank+1>_IO<n> 
// where banks are 1-based indexed
// eg.: GPIO5_IO07: 8th signal in 5th GPIO bank
// Despite that, this macro expects a 0-based bank index because 
// that is how GpioClx numbers GPIO banks
#define IMX_MAKE_PIN(BANK, IO)  (((BANK) * 32) + (IO))

enum : ULONG {
    IMX_GPIO_ALLOC_TAG = '6XMI'
};

enum {
#ifdef IMX6DQ
    // i.MX6 Dual/Quad specifics
    IMX_GPIO_PINS_PER_BANK = 32,
    IMX_GPIO_BANK_COUNT = 7,
    // GPIO7_IO13 is the highest GPIO signal
    IMX_GPIO_PIN_COUNT = 205
#else
#pragma error("i.MX6 variant not supported. Please define i.MX6 variant specific data")
#endif
};

static_assert(IMX_GPIO_PINS_PER_BANK <= 32, "Driver supports max of 32 pin per bank");

//
//   PUS [15:14] - Pull Up / Down Config. Field Reset: 100K_OHM_PU
//     100K_OHM_PD (0) - 100K Ohm Pull Down
//     47K_OHM_PU (1) - 47K Ohm Pull Up
//     100K_OHM_PU (2) - 100K Ohm Pull Up
//     22K_OHM_PU (3) - 22K Ohm Pull Up
//   PUE [13] - Pull / Keep Select Field Reset: PULL
//     KEEP (0) - Keeper Enabled
//     PULL (1) - Pull Enabled
//   PKE [12] - Pull / Keep Enable Field Reset: ENABLED
//     DISABLED (0) - Pull/Keeper Disabled
//     ENABLED (1) - Pull/Keeper Enabled
//
enum IMX_GPIO_PULL {
    IMX_GPIO_PULL_DISABLE = 0x0, // 0b0000
    IMX_GPIO_PULL_DOWN = 0x3, // 0b0011
    IMX_GPIO_PULL_UP = 0xB, // 0b1011
    IMX_GPIO_PULL_DEFAULT = 0xFFFFFFFF
};

#define IMX_GPIO_PULL_SHIFT 12
#define IMX_GPIO_PULL_MASK  (0b1111<<IMX_GPIO_PULL_SHIFT)

#include <pshpack1.h>  //====================================================

#ifdef IMX6DQ
struct IMX_IOMUXC_REGISTERS {
    ULONG Reg[596];
}; // struct IMX_IOMUXC_REGISTERS
#else
#pragma error("i.MX6 variant not supported. Please define i.MX6 variant specific data")
#endif

struct IMX_GPIO_BANK_REGISTERS {
    ULONG Data;                 // GPIOx_DR
    ULONG Direction;            // GPIOx_GDIR
    ULONG PadStatus;            // GPIOx_PSR
    ULONG InterruptConfig1;     // GPIOx_ICR1
    ULONG InterruptConfig2;     // GPIOx_ICR2
    ULONG InterruptMask;        // GPIOx_IMR
    ULONG InterruptStatus;      // GPIOx_ISR
    ULONG EdgeSelect;           // GPIOx_EDGE_SEL
}; // struct IMX_GPIO_BANK_REGISTERS

struct IMX_GPIO_REGISTERS {
    IMX_GPIO_BANK_REGISTERS Bank[IMX_GPIO_BANK_COUNT];
}; // struct IMX_GPIO_REGISTERS

#include <poppack.h> //======================================================

// Captures a logical pin's IOMUXC_SW_PAD_CTL_* data
struct IMX_PIN_DATA {
    ULONG PadCtlByteOffset;
    ULONG PadCtlDefault;
}; // IMX_PIN_DATA

class IMX_GPIO {
public: // NONPAGED

    static GPIO_CLIENT_READ_PINS_MASK ReadGpioPinsUsingMask;
    static GPIO_CLIENT_WRITE_PINS_MASK WriteGpioPinsUsingMask;

    static GPIO_CLIENT_START_CONTROLLER StartController;
    static GPIO_CLIENT_STOP_CONTROLLER StopController;

private: // NONPAGED

    enum class _SIGNATURE {
        UNINITIALIZED = '6xmi',
        CONSTRUCTED = '6XMI',
        DESTRUCTED = 0
    } signature;

    IMX_GPIO_REGISTERS* gpioRegsPtr;
    ULONG gpioRegsLength;
    IMX_IOMUXC_REGISTERS* iomuxcRegsPtr;
    ULONG iomuxcRegsLength;
    ULONG banksDataReg[IMX_GPIO_BANK_COUNT];

public: // PAGED

    static GPIO_CLIENT_QUERY_CONTROLLER_BASIC_INFORMATION
        QueryControllerBasicInformation;

    static GPIO_CLIENT_PREPARE_CONTROLLER PrepareController;
    static GPIO_CLIENT_RELEASE_CONTROLLER ReleaseController;

    static GPIO_CLIENT_CONNECT_IO_PINS ConnectIoPins;
    static GPIO_CLIENT_DISCONNECT_IO_PINS DisconnectIoPins;

    static EVT_WDF_DRIVER_DEVICE_ADD EvtDriverDeviceAdd;
    static EVT_WDF_DRIVER_UNLOAD EvtDriverUnload;

private: // PAGED

    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS updatePullMode (
        ULONG AbsolutePinNumber,
        IMX_GPIO_PULL PullMode
        );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    IMX_GPIO (
        _In_ IMX_IOMUXC_REGISTERS* IomuxcRegsPtr,
        ULONG IomuxcRegsLength,
        _In_ IMX_GPIO_REGISTERS* GpioRegsPtr,
        ULONG GpioRegsLength
        );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    ~IMX_GPIO ();

    IMX_GPIO (const IMX_GPIO&) = delete;
    IMX_GPIO& operator= (const IMX_GPIO&) = delete;
}; // class IMX_GPIO

extern "C" DRIVER_INITIALIZE DriverEntry;

#endif // _IMX6GPIO_HPP_
