//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011hw.cpp
//
// Abstract:
//
//    This module contains methods for accessing the ARM PL011
//    UART controller hardware.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//
#include "precomp.h"
#pragma hdrstop

#define _PL011_HW_CPP_

// Logging header files
#include "PL011logging.h"
#include "PL011hw.tmh"

// Common driver header files
#include "PL011common.h"

// Module specific header files


//
// Routine Description:
//
//  PL011HwInitController initializes the ARM PL011 controller, and  
//  sets it to a known state.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
// Return Value:
//
//  Controller initialization status.
//
_Use_decl_annotations_
NTSTATUS
PL011HwInitController(
    WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);

    //
    // Disable the UART 
    //
    PL011HwUartControl(
        WdfDevice,
        UARTCR_ALL, // All
        REG_UPDATE_MODE::BITMASK_CLEAR,
        nullptr
        );

    //
    // Disable interrupts
    //
    PL011HwMaskInterrupts(
        WdfDevice,
        UART_INTERUPPTS_ALL, // All
        TRUE, // Mask,
        TRUE  // ISR safe
        );

    //
    // Clear pending interrupts, if any...
    //
    PL011HwClearInterrupts(
        devExtPtr, 
        UART_INTERUPPTS_ALL
        );

    //
    // Clear any errors, if any...
    //
    PL011HwClearRxErros(devExtPtr);

    //
    // Configure FIFOs threshold
    //
    PL011HwSetFifoThreshold(
        WdfDevice,
        UARTIFLS_RXIFLSEL::RXIFLSEL_1_4, // RX FIFO threshold >= 1/4 full
        UARTIFLS_TXIFLSEL::TXIFLSEL_1_8  // TX FIFO threshold <= 1/8 full
        );

    //
    // Get supported baud rates if it has not 
    // already done.
    //
    PL011HwGetSupportedBaudRates(WdfDevice);

    //
    // Enable UART. 
    // RX and TX are enabled when the device is opened.
    //
    PL011HwUartControl(
        WdfDevice,
        UARTCR_UARTEN,
        REG_UPDATE_MODE::OVERWRITE,
        nullptr
        );

    PL011_LOG_INFORMATION("Controller initialization done!");

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011HwStopController initializes stops the ARM PL011 controller, and  
//  resets it to a known state.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011HwStopController(
    WDFDEVICE WdfDevice
    )
{
    //
    // Disable the UART 
    //
    PL011HwUartControl(
        WdfDevice,
        UARTCR_ALL, // All
        REG_UPDATE_MODE::BITMASK_CLEAR,
        nullptr
        );

    //
    // Disable interrupts
    //
    PL011HwMaskInterrupts(
        WdfDevice,
        UART_INTERUPPTS_ALL, // All
        TRUE, // Mask,
        TRUE  // ISR safe
        );

    PL011_LOG_INFORMATION("Controller stop done!");
}


//
// Routine Description:
//
//  PL011HwGetSupportedBaudRates is called to find out what baud rates
//  are supported, given the UART clock.
//
//  The actual supported baud rate investigation is done once per the 
//  lifetime of the device, if the settable baud rates investigation 
//  was already done, routine does nothing.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011HwGetSupportedBaudRates(
    WDFDEVICE WdfDevice
    )
{
    //
    // Baud rate 
    //
    struct PL011_BAUD_RATE_DESCRIPTOR
    {
        // BAUD rate code SERIAL_BAUD_???
        ULONG   BaudCode;

        // Baud rate value in BPS
        ULONG   BaudBPS;

    } static baudValues[] = {
        { SERIAL_BAUD_110,      110 },
        { SERIAL_BAUD_150,      150 },
        { SERIAL_BAUD_300,      300 },
        { SERIAL_BAUD_600,      600 },
        { SERIAL_BAUD_1200,     1200 },
        { SERIAL_BAUD_1800,     1800 },
        { SERIAL_BAUD_2400,     2400 },
        { SERIAL_BAUD_4800,     4800 },
        { SERIAL_BAUD_9600,     9600 },
        { SERIAL_BAUD_14400,    14400 },
        { SERIAL_BAUD_19200,    19200 },
        { SERIAL_BAUD_38400,    38400 },
        { SERIAL_BAUD_57600,    57600 },
        { SERIAL_BAUD_115200 ,  115200 },
    }; // baudValues

    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    if (devExtPtr->SettableBaud != 0) {

        return;
    }
    devExtPtr->SettableBaud = SERIAL_BAUD_USER;

    for (ULONG descInx = 0; descInx < ULONG(ARRAYSIZE(baudValues)); ++descInx) {

        PL011_BAUD_RATE_DESCRIPTOR* baudDescPtr = &baudValues[descInx];

        NTSTATUS status = PL011HwSetBaudRate(WdfDevice, baudDescPtr->BaudBPS);
        if (NT_SUCCESS(status)) {

            devExtPtr->SettableBaud |= baudDescPtr->BaudCode;
        }

    } // for (baud rate descriptors)
}


//
// Routine Description:
//
//  PL011HwUartControl is called to modify the UART control register.
//  The routine either enable or disable controls, based on IsSetControl.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  UartControlMask - A combination of UARTCR_??? bits.
//
//  RegUpdateMode - How to update the control register.
//
//  OldUartControlPtr - Address of a caller ULONG var to receive the
//      old UART control, or nullptr if not needed.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011HwUartControl(
    WDFDEVICE WdfDevice,
    ULONG UartControlMask,
    REG_UPDATE_MODE RegUpdateMode,
    ULONG* OldUartControlPtr
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    volatile ULONG* regUARTCRPtr = PL011HwRegAddress(devExtPtr, UARTCR);

    //
    // Update UARTCR
    //

    KLOCK_QUEUE_HANDLE lockHandle;
    KeAcquireInStackQueuedSpinLock(&devExtPtr->RegsLock, &lockHandle);

    ULONG regUARTCR = PL011HwReadRegisterUlong(regUARTCRPtr);

    if (OldUartControlPtr != nullptr) {

        *OldUartControlPtr = regUARTCR;
    }

    switch (RegUpdateMode) {
    case REG_UPDATE_MODE::BITMASK_SET:
        regUARTCR |= UartControlMask;
        break;

    case REG_UPDATE_MODE::BITMASK_CLEAR:
        regUARTCR &= ~UartControlMask;
        break;

    case REG_UPDATE_MODE::QUERY:
        goto done;

    case REG_UPDATE_MODE::OVERWRITE:
        regUARTCR = UartControlMask;
        break;

    default:
        PL011_LOG_ERROR(
            "Invalid register update mode %d", 
            ULONG(RegUpdateMode)
            );
        RegUpdateMode = REG_UPDATE_MODE::INVALID;
        goto done;
    }
    regUARTCR &= UARTCR_ALL;

    PL011HwWriteRegisterUlong(
        regUARTCRPtr,
        regUARTCR
        );

done:

    KeReleaseInStackQueuedSpinLock(&lockHandle);
    
    //
    // Log new UART control information
    //
    {
        static char* updateModeStrings[] = {
            "INVALID",
            "BITMASK_SET",
            "BITMASK_CLEAR",
            "QUERY",
            "OVERWRITE"
        };

        PL011_LOG_INFORMATION(
            "UART Control: update mode '%s', mask 0x%04X, actual 0x%04X",
            updateModeStrings[ULONG(RegUpdateMode)],
            USHORT(UartControlMask),
            USHORT(PL011HwReadRegisterUlong(regUARTCRPtr))
            );

    } // Log new UART control information
}


//
// Routine Description:
//
//  PL011HwSetFifoThreshold is called to set the UART FIFOs occupancy 
//  threshold for triggering RX/TX interrupts.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  RxInterruptTrigger - RX FIFO occupancy threshold interrupt trigger.
//
//  TxInterruptTrigger - TX FIFO occupancy threshold interrupt trigger.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011HwSetFifoThreshold(
    WDFDEVICE WdfDevice,
    UARTIFLS_RXIFLSEL RxInterruptTrigger,
    UARTIFLS_TXIFLSEL TxInterruptTrigger
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    volatile ULONG* regUARTIFLSPtr = PL011HwRegAddress(devExtPtr, UARTIFLS);

    PL011HwEnableFifos(WdfDevice, FALSE);

    //
    // Update the Interrupt FIFO level select register, UARTIFLS
    //
    {
        KLOCK_QUEUE_HANDLE lockHandle;
        KeAcquireInStackQueuedSpinLock(&devExtPtr->RegsLock, &lockHandle);

        ULONG regUARTIFLS = PL011HwReadRegisterUlong(regUARTIFLSPtr);

        regUARTIFLS &= ~(UARTIFLS_TXIFLSEL_MASK | UARTIFLS_RXIFLSEL_MASK);
        regUARTIFLS |= (ULONG(RxInterruptTrigger) | ULONG(TxInterruptTrigger));

        PL011HwWriteRegisterUlong(regUARTIFLSPtr, regUARTIFLS);

        KeReleaseInStackQueuedSpinLock(&lockHandle);

    } // Update the Interrupt FIFO level select register, UARTIFLS

    PL011HwEnableFifos(WdfDevice, TRUE);

    PL011_LOG_INFORMATION(
        "UART FIFO triggers set to RX %lu, TX %lu, UARTIFLS 0x%04X",
        int(RxInterruptTrigger),
        int(TxInterruptTrigger),
        USHORT(PL011HwReadRegisterUlong(regUARTIFLSPtr))
        );
}


//
// Routine Description:
//
//  PL011HwMaskInterrupts is called to mask/unmask UART interrupts.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  InterruptBitMask - A combination of interrupt types to mask/unmask,
//      please refer to UARTIMSC_??? definitions for the various types
//      of interrupts.
//
//  IsMaskInterrupts - If to mask interrupts (TRUE), or unmask (FALSE).
//
//  IsIsrSafe - If to sync with ISR (TRUE), or not (FALSE). Usually
//      IsIsrSafe is set to FALSE when called from ISR, or from
//      framework Interrupt control callbacks.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011HwMaskInterrupts(
    WDFDEVICE WdfDevice,
    ULONG InterruptBitMask,
    BOOLEAN IsMaskInterrupts,
    BOOLEAN IsIsrSafe
    )
{
    PL011_ASSERT(
        ((IsIsrSafe == TRUE) && (KeGetCurrentIrql() <= DISPATCH_LEVEL)) ||
        ((IsIsrSafe == FALSE) && (KeGetCurrentIrql() > DISPATCH_LEVEL))
        );

    //
    // Update the Interrupt mask set/clear register, UARTIMSC
    //

    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    volatile ULONG* regUARTIMSCPtr = PL011HwRegAddress(devExtPtr, UARTIMSC);

    //
    // The following code should be synchronized with ISR code.
    //
    {
        if (IsIsrSafe) {
            WdfInterruptAcquireLock(devExtPtr->WdfUartInterrupt);
        }

        ULONG regUARTIMSC = PL011HwReadRegisterUlong(regUARTIMSCPtr);
        ULONG oldRegUARTIMSC = regUARTIMSC;

        if (IsMaskInterrupts) {
            regUARTIMSC &= ~InterruptBitMask;
        }
        else {
            regUARTIMSC |= InterruptBitMask;
        }
        regUARTIMSC &= UART_INTERUPPTS_ALL;

        PL011HwWriteRegisterUlong(regUARTIMSCPtr, regUARTIMSC);

        if (IsIsrSafe) {
            WdfInterruptReleaseLock(devExtPtr->WdfUartInterrupt);
        }

        PL011_LOG_TRACE(
            "%s events, old 0x%04X, mask 0x%04X, new 0x%04X",
            IsMaskInterrupts ? "Disabling" : "Enabling",
            USHORT(oldRegUARTIMSC),
            USHORT(InterruptBitMask),
            USHORT(PL011HwReadRegisterUlong(regUARTIMSCPtr))
            );

    } // ISR safe code
}


//
// Routine Description:
//
//  PL011HwSetBaudRate is called to configure the UART to a new baud rate.
//  The routine checks if the desired baud rate is supported:
//  - Desired baud rate is in range.
//  - Given the UART clock the error is within the allowed tolerance. 
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  BaudRateBPS - The desired baud rate in Bits Per Second (BPS)
//
// Return Value:
//
//  STATUS_SUCCESS, or STATUS_NOT_SUPPORTED if the desired 
//  baud rate is not supported.
//
_Use_decl_annotations_
NTSTATUS
PL011HwSetBaudRate(
    WDFDEVICE WdfDevice,
    ULONG BaudRateBPS
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);

    //
    // 1) Validate baud rate is in range
    //
    if ((BaudRateBPS < PL011_MIN_BAUD_RATE_BPS) ||
        (BaudRateBPS > devExtPtr->CurrentConfiguration.MaxBaudRateBPS)) {

        PL011_LOG_ERROR(
            "Baud rate out of range %lu, (%lu..%lu)",
            BaudRateBPS,
            PL011_MIN_BAUD_RATE_BPS,
            devExtPtr->CurrentConfiguration.MaxBaudRateBPS
            );
        return STATUS_NOT_SUPPORTED;
    }

    //
    // 2) Make sure requested baud rate is valid.
    //    UART clock should be at least 16 times the baud rate
    //
    ULONG uartClockHz = devExtPtr->CurrentConfiguration.UartClockHz;
    if (BaudRateBPS > (uartClockHz / 16)) {

        PL011_LOG_ERROR(
            "Requested baud rate %lu should be less than UART clock (%lu) / 16",
            BaudRateBPS,
            uartClockHz
            );
        return STATUS_NOT_SUPPORTED;
    }

    //
    // 3) Calculate baud rate divisor:
    //
    //  BaudDivsior = UartCLockHz / (16 * BaudRateBPS)
    //  Where
    //    - UARTIBRD (16 bits) is the integer part of BaudDivisor.
    //    - UARTFBRD (6 bits) is the fractional part of BaudDivisor.
    //
    ULONG baudDivisor = uartClockHz * 4 / BaudRateBPS;

    //
    // Calculate UARTIBRD
    //
    ULONG regUARTIBRD = baudDivisor >> 6;
    //
    // Calculate UARTFBRD, the fraction part, uses block floating point
    // with 6 bits (64).
    //
    ULONG regUARTFBRD = baudDivisor & ULONG(0x3F);

    //
    // 4) Calculate the error, and make sure it is within the allowed range
    //
    ULONG actualBaudRateBPS = uartClockHz * 4 / (64 * regUARTIBRD + regUARTFBRD);
    int baudRateErrorPercent = int(actualBaudRateBPS - BaudRateBPS);
    if (baudRateErrorPercent < 0) {
        baudRateErrorPercent *= -1;
    }
    baudRateErrorPercent = baudRateErrorPercent * 100 / BaudRateBPS;

    if (baudRateErrorPercent > PL011_MAX_BUAD_RATE_ERROR_PERCENT) {

        PL011_LOG_ERROR(
            "Baud rate error out of range %lu%% > Max (%lu%%)",
            baudRateErrorPercent,
            PL011_MAX_BUAD_RATE_ERROR_PERCENT
            );
        return STATUS_NOT_SUPPORTED;
    }

    //
    // 5) Write to HW  
    //
    {
        KLOCK_QUEUE_HANDLE lockHandle;
        KeAcquireInStackQueuedSpinLock(&devExtPtr->RegsLock, &lockHandle);

        ULONG regUARTLCR_H = PL011HwReadRegisterUlong(
            PL011HwRegAddress(devExtPtr, UARTLCR_H)
            );

        PL011HwWriteRegisterUlong(
            PL011HwRegAddress(devExtPtr, UARTIBRD),
            regUARTIBRD
            );
        PL011HwWriteRegisterUlong(
            PL011HwRegAddress(devExtPtr, UARTFBRD),
            regUARTFBRD
            );

        PL011HwWriteRegisterUlong(
            PL011HwRegAddress(devExtPtr, UARTLCR_H),
            regUARTLCR_H
            );

        KeReleaseInStackQueuedSpinLock(&lockHandle);

    } // Write to HW  

    //
    // 6) Update current configuration
    //
    {
        KIRQL oldIrql = ExAcquireSpinLockExclusive(&devExtPtr->ConfigLock);

        devExtPtr->CurrentConfiguration.UartSerialBusDescriptor.BaudRate = 
            BaudRateBPS;

        ExReleaseSpinLockExclusive(&devExtPtr->ConfigLock, oldIrql);

    } // Get current baud rate

    PL011_LOG_INFORMATION(
        "Baud rate was successfully set to %lu [BPS], "
        "UARTIBRD 0x%08X, regUARTFBRD 0x%08X",
        BaudRateBPS,
        regUARTIBRD,
        regUARTFBRD
        );

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011HwSetFlowControl is called to configure the UART flow control.
//  The routine verifies the desired flow control setup is supported,
//  and applies it to the UART.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  SerialFlowControlPtr - Address of a caller SERIAL_HANDFLOW var that
//      contains the desired flow control setup.
//
// Return Value:
//
//  STATUS_SUCCESS, or STATUS_NOT_SUPPORTED if the desired 
//  flow control setup is not supported by the SoC.
//
_Use_decl_annotations_
NTSTATUS
PL011HwSetFlowControl(
    WDFDEVICE WdfDevice,
    const SERIAL_HANDFLOW* SerialFlowControlPtr
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);

    //
    // Get current UART control
    //
    ULONG regUARTCR = 0;
    PL011HwUartControl(
        WdfDevice,
        0,
        REG_UPDATE_MODE::QUERY,
        &regUARTCR
        );

    regUARTCR &= ~UARTCR_CTSEn;
    if ((SerialFlowControlPtr->ControlHandShake & SERIAL_CTS_HANDSHAKE) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_CTSEn) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_CTSEn;

    } // SERIAL_CTS_HANDSHAKE

    regUARTCR &= ~UARTCR_RTSEn;
    if ((SerialFlowControlPtr->FlowReplace & SERIAL_RTS_HANDSHAKE) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_RTSEn) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_RTSEn;

    } // SERIAL_RTS_HANDSHAKE

    regUARTCR &= ~UARTCR_RTS;
    if ((SerialFlowControlPtr->FlowReplace & SERIAL_RTS_CONTROL) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_RTS) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_RTS;

    } // SERIAL_RTS_CONTROL

    regUARTCR &= ~UARTCR_DTR;
    if ((SerialFlowControlPtr->ControlHandShake & SERIAL_DTR_CONTROL) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_DTR) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_DTR;

    } // SERIAL_DTR_CONTROL

    //
    // Now set the new UART control
    //
    PL011HwUartControl(
        WdfDevice,
        regUARTCR,
        REG_UPDATE_MODE::OVERWRITE,
        nullptr
        );

    //
    // Save new flow control setup
    //
    {
        KIRQL oldIrql = ExAcquireSpinLockExclusive(&devExtPtr->ConfigLock);

        devExtPtr->CurrentConfiguration.FlowControlSetup =
            *SerialFlowControlPtr;

        ExReleaseSpinLockExclusive(&devExtPtr->ConfigLock, oldIrql);

    } // Get current line control setup

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011HwSetLineControl is called to configure the UART line control.
//  The routine maps the UART generic settings into PL011 registers
//  and applies the new setup.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  SerialLineControlPtr - Address of a caller SERIAL_LINE_CONTROL var that
//      contains the desired line control setup.
//
// Return Value:
//
//  STATUS_SUCCESS, STATUS_NOT_SUPPORTED if the desired line control setup 
//  is not supported, STATUS_INVALID_PARAMETER.
//
_Use_decl_annotations_
NTSTATUS
PL011HwSetLineControl(
    WDFDEVICE WdfDevice,
    const SERIAL_LINE_CONTROL* SerialLineControlPtr
    )
{
    NTSTATUS status = STATUS_NOT_SUPPORTED;
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    volatile ULONG* regUARTLCR_HPtr = PL011HwRegAddress(devExtPtr, UARTLCR_H);

    //
    // Disable the UART 
    //
    ULONG regUARTCR;
    PL011HwUartControl(
        WdfDevice,
        UARTCR_UARTEN,
        REG_UPDATE_MODE::BITMASK_CLEAR,
        &regUARTCR
        );

    KLOCK_QUEUE_HANDLE lockHandle;
    KeAcquireInStackQueuedSpinLock(&devExtPtr->RegsLock, &lockHandle);
   
    ULONG regUARTLCR_H = PL011HwReadRegisterUlong(regUARTLCR_HPtr);

    //
    // Set word length
    //
    regUARTLCR_H &= ~UARTLCR_WLEN_MASK;

    switch (SerialLineControlPtr->WordLength) {

    case 5:
        regUARTLCR_H |= ULONG(UARTLCR_WLEN::WLEN_5BITS);
        break;

    case 6:
        regUARTLCR_H |= ULONG(UARTLCR_WLEN::WLEN_6BITS);
        break;

    case 7:
        regUARTLCR_H |= ULONG(UARTLCR_WLEN::WLEN_7BITS);
        break;

    case 8:
        regUARTLCR_H |= ULONG(UARTLCR_WLEN::WLEN_8BITS);
        break;

    default:
        status = STATUS_NOT_SUPPORTED;
        goto done;

    } // switch (WordLength)

    //
    // Set stop bits
    //
    int stopBits;
    switch (SerialLineControlPtr->StopBits) {

    case STOP_BIT_1:
        regUARTLCR_H &= ~UARTLCR_STP2;
        stopBits = 1;
        break;

    case STOP_BITS_2:
        regUARTLCR_H |= UARTLCR_STP2;
        stopBits = 2;
        break;

    default:
        status = STATUS_NOT_SUPPORTED;
        goto done;

    } // switch (StopBits)

    //
    // Parity
    //
    const char* partityStrSz;
    switch (SerialLineControlPtr->Parity) {

    case NO_PARITY:
        regUARTLCR_H &= ~UARTLCR_PEN;
        partityStrSz = "NONE";
        break;

    case ODD_PARITY:
        regUARTLCR_H |= UARTLCR_PEN; 
        regUARTLCR_H &= ~UARTLCR_EPS;
        regUARTLCR_H &= ~UARTLCR_SPS;
        partityStrSz = "ODD";
        break;

    case EVEN_PARITY:
        regUARTLCR_H |= UARTLCR_PEN;
        regUARTLCR_H |= UARTLCR_EPS;
        regUARTLCR_H &= ~UARTLCR_SPS;
        partityStrSz = "EVEN";
        break;

    case MARK_PARITY:
        regUARTLCR_H |= UARTLCR_PEN;
        regUARTLCR_H &= ~UARTLCR_EPS;
        regUARTLCR_H |= UARTLCR_SPS;
        partityStrSz = "MARK";
        break;

    case SPACE_PARITY:
        regUARTLCR_H |= UARTLCR_PEN;
        regUARTLCR_H |= UARTLCR_EPS;
        regUARTLCR_H |= UARTLCR_SPS;
        partityStrSz = "SPACE";
        break;

    default:
        status = STATUS_INVALID_PARAMETER;
        goto done;

    } // switch (Parity)

    //
    // Apply new line control setup
    //
    PL011HwWriteRegisterUlong(regUARTLCR_HPtr, regUARTLCR_H);

    //
    // Save current line control setup
    //
    {
        KIRQL oldIrql = ExAcquireSpinLockExclusive(&devExtPtr->ConfigLock);

        devExtPtr->CurrentConfiguration.LineControlSetup =
            *SerialLineControlPtr;

        ExReleaseSpinLockExclusive(&devExtPtr->ConfigLock, oldIrql);

    } // Get current line control setup

    status = STATUS_SUCCESS;

    PL011_LOG_INFORMATION(
        "UART Line Control successfully set to %lu bits, %lu stop bits, parity %s",
        SerialLineControlPtr->WordLength,
        stopBits,
        partityStrSz
        );

done:

    KeReleaseInStackQueuedSpinLock(&lockHandle);

    PL011HwUartControl(
        WdfDevice,
        regUARTCR, // Restore
        REG_UPDATE_MODE::OVERWRITE,
        nullptr
        );

    return status;
}


//
// Routine Description:
//
//  PL011HwEnableFifos is called to enable/disable RX/TX FIFOs.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  IsEnable - If to enable (TRUE), or disable (FALSE) FIFOs
//
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011HwEnableFifos(
    WDFDEVICE WdfDevice,
    BOOLEAN IsEnable
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    volatile ULONG* regUARTLCR_HPtr = PL011HwRegAddress(devExtPtr, UARTLCR_H);

    KLOCK_QUEUE_HANDLE lockHandle;
    KeAcquireInStackQueuedSpinLock(&devExtPtr->RegsLock, &lockHandle);

    ULONG regUARTLCR_H = PL011HwReadRegisterUlong(regUARTLCR_HPtr);

    if (IsEnable) {

        regUARTLCR_H |= UARTLCR_FEN;

    } else {

        regUARTLCR_H &= ~UARTLCR_FEN;
    }

    PL011HwWriteRegisterUlong(regUARTLCR_HPtr, regUARTLCR_H);

    KeReleaseInStackQueuedSpinLock(&lockHandle);
}


//
// Routine Description:
//
//  PL011HwSetModemControl is called to assert modem control signals.
//  The routine verifies that modem control is supported by the SoC
//  UART and applies the setting to the hardware.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  ModemControl - Please refer to  SERIAL_MCR_??? for bit definitions.
//
// Return Value:
//  
//  STATUS_SUCCESS, or STATUS_NOT_SUPPORTED if current SoC does not expose
//      modem control lines.
//
_Use_decl_annotations_
NTSTATUS
PL011HwSetModemControl(
    WDFDEVICE WdfDevice,
    UCHAR ModemControl
    )
{
    const PL011_DEVICE_EXTENSION* devExtPtr = 
        PL011DeviceGetExtension(WdfDevice);

    //
    // Get current UART control
    //
    ULONG regUARTCR = 0;
    PL011HwUartControl(
        WdfDevice,
        0,
        REG_UPDATE_MODE::QUERY,
        &regUARTCR
        );

    regUARTCR &= ~UARTCR_CTSEn;
    if ((ModemControl & SERIAL_MCR_CTS_EN) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_CTSEn) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_CTSEn;

    } // SERIAL_CTS_HANDSHAKE

    regUARTCR &= ~UARTCR_RTSEn;
    if ((ModemControl & SERIAL_MCR_RTS_EN) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_RTSEn) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_RTSEn;

    } // SERIAL_RTS_HANDSHAKE

    regUARTCR &= ~UARTCR_RTS;
    if ((ModemControl & SERIAL_MCR_RTS) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_RTS) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_RTS;

    } // SERIAL_RTS_CONTROL

    regUARTCR &= ~UARTCR_DTR;
    if ((ModemControl & SERIAL_MCR_DTR) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_DTR) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_DTR;

    } // SERIAL_DTR_CONTROL

    regUARTCR &= ~UARTCR_Out1;
    if ((ModemControl & SERIAL_MCR_OUT1) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_Out1) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_Out1;

    } // SERIAL_MCR_OUT1

    regUARTCR &= ~UARTCR_Out2;
    if ((ModemControl & SERIAL_MCR_OUT2) != 0) {

        if ((devExtPtr->UartSupportedControlsMask & UARTCR_Out2) == 0) {

            return STATUS_NOT_SUPPORTED;
        }
        regUARTCR |= UARTCR_Out2;

    } // SERIAL_MCR_OUT2

    regUARTCR &= ~UARTCR_LBE;
    if ((ModemControl & SERIAL_MCR_LOOP) != 0) {

        regUARTCR |= UARTCR_LBE;

    } // SERIAL_MCR_LOOP

    PL011HwUartControl(
        WdfDevice,
        regUARTCR,
        REG_UPDATE_MODE::OVERWRITE,
        nullptr
        );

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011HwGetModemControl is called to query modem control signals state.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  ModemControlPtr - Address of a caller UCGAR to receive the current 
//      modem control.
//      Please refer to  SERIAL_MCR_??? for bit definitions.
//
// Return Value:
//  
//  STATUS_SUCCESS.
//
_Use_decl_annotations_
NTSTATUS
PL011HwGetModemControl(
    WDFDEVICE WdfDevice,
    UCHAR* ModemControlPtr
    )
{
    *ModemControlPtr = 0;

    //
    // Get current UART control
    //
    UCHAR modemControl = 0;
    ULONG regUARTCR;
    PL011HwUartControl(
        WdfDevice,
        0,
        REG_UPDATE_MODE::QUERY,
        &regUARTCR
        );

    if ((regUARTCR & UARTCR_CTSEn) != 0) {

        modemControl |= SERIAL_MCR_CTS_EN;
    }

    if ((regUARTCR & UARTCR_RTSEn) != 0) {

        modemControl |= SERIAL_MCR_RTS_EN;
    }

    if ((regUARTCR & UARTCR_RTS) != 0) {

        modemControl |= SERIAL_MCR_RTS;
    }

    if ((regUARTCR & UARTCR_DTR) != 0) {

        modemControl |= SERIAL_MCR_DTR;
    }

    if ((regUARTCR & UARTCR_LBE) != 0) {

        modemControl |= SERIAL_MCR_LOOP;
    }

    if ((regUARTCR & UARTCR_Out1) != 0) {

        modemControl |= SERIAL_MCR_OUT1;
    }

    if ((regUARTCR & UARTCR_Out2) != 0) {

        modemControl |= SERIAL_MCR_OUT2;
    }

    *ModemControlPtr = modemControl;

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011HwSetBreak is called to send/clear break.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  IsBreakON - If to send (TRUE), or clear (FALSE), break.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011HwSetBreak(
    WDFDEVICE WdfDevice,
    BOOLEAN IsBreakON
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    volatile ULONG* regUARTLCR_HPtr = PL011HwRegAddress(devExtPtr, UARTLCR_H);

    KLOCK_QUEUE_HANDLE lockHandle;
    KeAcquireInStackQueuedSpinLock(&devExtPtr->RegsLock, &lockHandle);

    ULONG regUARTLCR_H = PL011HwReadRegisterUlong(regUARTLCR_HPtr);

    if (IsBreakON) {

        regUARTLCR_H |= UARTLCR_BRK;

    } else {

        regUARTLCR_H &= ~UARTLCR_BRK;
    }

    PL011HwWriteRegisterUlong(regUARTLCR_HPtr, regUARTLCR_H);

    KeReleaseInStackQueuedSpinLock(&lockHandle);
}


//
// Routine Description:
//
//  PL011HwRegsDump is called to dump important PL011 registers.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011HwRegsDump(
    WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);

    PL011_LOG_TRACE(
        "UARTCR %04X, "
        "UARTIBRD %04X, "
        "UARTFBRD %04X, "
        "UARTLCR_H %04X, "
        "UARTIMSC %04X, "
        "UARTIFLS %04X, "
        "UARTRIS %04X, "
        "UARTFR %04X",
        PL011HwReadRegisterUlong(PL011HwRegAddress(devExtPtr, UARTCR)),
        PL011HwReadRegisterUlong(PL011HwRegAddress(devExtPtr, UARTIBRD)),
        PL011HwReadRegisterUlong(PL011HwRegAddress(devExtPtr, UARTFBRD)),
        PL011HwReadRegisterUlong(PL011HwRegAddress(devExtPtr, UARTLCR_H)),
        PL011HwReadRegisterUlong(PL011HwRegAddress(devExtPtr, UARTIMSC)),
        PL011HwReadRegisterUlong(PL011HwRegAddress(devExtPtr, UARTIFLS)),
        PL011HwReadRegisterUlong(PL011HwRegAddress(devExtPtr, UARTRIS)),
        PL011HwReadRegisterUlong(PL011HwRegAddress(devExtPtr, UARTFR))
        );
}


#undef _PL011_HW_CPP_