//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011ioctl.h
//
// Abstract:
//
//    This module contains implementation of IOCTL request handlers
//    associated with the ARM PL011 UART device driver.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//
#include "precomp.h"
#pragma hdrstop

#define _PL011_IOCTL_CPP_

// Logging header files
#include "PL011logging.h"
#include "PL011ioctl.tmh"

// Common driver header files
#include "PL011common.h"

// Module specific header files
#include "PL011ioctl.h"
#include "PL011rx.h"
#include "PL011tx.h"


//
// Routine Description:
//
//  PL011IoctlSetBaudRate is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_SET_BAUD_RATE IO control request.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or not supported if the desired baud rate is
//      not supported.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlSetBaudRate(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;

    PSERIAL_BAUD_RATE serialBaudRatePtr;
    status = WdfRequestRetrieveInputBuffer(
        WdfRequest,
        sizeof(SERIAL_BAUD_RATE),
        reinterpret_cast<PVOID*>(&serialBaudRatePtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_BAUD_RATE buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

    status = PL011HwSetBaudRate(WdfDevice, serialBaudRatePtr->BaudRate);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_SET_BAUD_RATE: %lu [BPS], (status = %!STATUS!)",
        serialBaudRatePtr->BaudRate,
        status
        );

done:

    WdfRequestComplete(WdfRequest, status);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlGetBaudRate is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_GET_BAUD_RATE IO control request.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlGetBaudRate(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;
    ULONG_PTR reqStatusInfo = 0;

    PSERIAL_BAUD_RATE serialBaudRatePtr;
    status = WdfRequestRetrieveOutputBuffer(
        WdfRequest,
        sizeof(SERIAL_BAUD_RATE),
        reinterpret_cast<PVOID*>(&serialBaudRatePtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_BAUD_RATE buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

    //
    // Get current baud rate
    //
    {
        PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
        KIRQL oldIrql = ExAcquireSpinLockShared(&devExtPtr->ConfigLock);

        serialBaudRatePtr->BaudRate =
            devExtPtr->CurrentConfiguration.UartSerialBusDescriptor.BaudRate;

        ExReleaseSpinLockShared(&devExtPtr->ConfigLock, oldIrql);

    } // Get current baud rate

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_GET_BAUD_RATE: %lu [BPS], (status = %!STATUS!)",
        serialBaudRatePtr->BaudRate,
        status
        );

    status = STATUS_SUCCESS;
    reqStatusInfo = sizeof(SERIAL_BAUD_RATE);

done:

    WdfRequestCompleteWithInformation(WdfRequest, status, reqStatusInfo);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlSetHandflow is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_SET_HANDFLOW IO control request.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlSetHandflow(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;

    PSERIAL_HANDFLOW serialHandFlowPtr;
    status = WdfRequestRetrieveInputBuffer(
        WdfRequest,
        sizeof(SERIAL_HANDFLOW),
        reinterpret_cast<PVOID*>(&serialHandFlowPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_HANDFLOW buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

    status = PL011HwSetFlowControl(WdfDevice, serialHandFlowPtr);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_SET_HANDFLOW: ControlHandShake 0x%08X, FlowReplace 0x%08X, (status = %!STATUS!)",
        serialHandFlowPtr->ControlHandShake,
        serialHandFlowPtr->FlowReplace,
        status
        );

done:

    WdfRequestComplete(WdfRequest, status);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlGetHandflow is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_GET_HANDFLOW IO control request.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlGetHandflow(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;
    ULONG_PTR reqStatusInfo = 0;

    PSERIAL_HANDFLOW serialHandFlowPtr;
    status = WdfRequestRetrieveOutputBuffer(
        WdfRequest,
        sizeof(SERIAL_HANDFLOW),
        reinterpret_cast<PVOID*>(&serialHandFlowPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_HANDFLOW buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

    //
    // Get current flow control setup
    //
    {
        PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
        KIRQL oldIrql = ExAcquireSpinLockShared(&devExtPtr->ConfigLock);

        *serialHandFlowPtr =
            devExtPtr->CurrentConfiguration.FlowControlSetup;

        ExReleaseSpinLockShared(&devExtPtr->ConfigLock, oldIrql);

    } // Get current flow control setup

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_GET_HANDFLOW: ControlHandShake 0x%08X, FlowReplace 0x%08X, (status = %!STATUS!)",
        serialHandFlowPtr->ControlHandShake,
        serialHandFlowPtr->FlowReplace,
        status
        );

    status = STATUS_SUCCESS;
    reqStatusInfo = sizeof(SERIAL_HANDFLOW);

done:

    WdfRequestCompleteWithInformation(WdfRequest, status, reqStatusInfo);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlSetModemControl is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_SET_MODEM_CONTROL IO control request.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlSetModemControl(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;

    ULONG* modemControlPtr;
    status = WdfRequestRetrieveInputBuffer(
        WdfRequest,
        sizeof(ULONG),
        reinterpret_cast<PVOID*>(&modemControlPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid modem control buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

    status = PL011HwSetModemControl(WdfDevice, *reinterpret_cast<UCHAR*>(modemControlPtr));

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_SET_MODEM_CONTROL: Modem Control 0x%01X, (status = %!STATUS!)",
        *reinterpret_cast<UCHAR*>(modemControlPtr),
        status
        );

done:

    WdfRequestComplete(WdfRequest, status);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlGetModemControl is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_GET_MODEM_CONTROL IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlGetModemControl(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;
    ULONG_PTR reqStatusInfo = 0;

    ULONG* modemControlPtr;
    status = WdfRequestRetrieveOutputBuffer(
        WdfRequest,
        sizeof(ULONG),
        reinterpret_cast<PVOID*>(&modemControlPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_HANDFLOW buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

    status = PL011HwGetModemControl(
        WdfDevice, 
        reinterpret_cast<UCHAR*>(modemControlPtr)
        );

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_GET_MODEM_CONTROL: 0x%1X (status = %!STATUS!)",
        UCHAR(*modemControlPtr),
        status
        );

    status = STATUS_SUCCESS;
    reqStatusInfo = sizeof(ULONG);

done:

    WdfRequestCompleteWithInformation(WdfRequest, status, reqStatusInfo);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlSetLineControl is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_SET_LINE_CONTROL IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlSetLineControl(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;

    PSERIAL_LINE_CONTROL serialLineControlPtr;
    status = WdfRequestRetrieveInputBuffer(
        WdfRequest,
        sizeof(SERIAL_LINE_CONTROL),
        reinterpret_cast<PVOID*>(&serialLineControlPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_LINE_CONTROL buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

    status = PL011HwSetLineControl(WdfDevice, serialLineControlPtr);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_SET_LINE_CONTROL: Word %lu, Stop Bits Code %lu, Parity Code %lu, (status = %!STATUS!)",
        serialLineControlPtr->WordLength,
        serialLineControlPtr->StopBits,
        serialLineControlPtr->Parity,
        status
        );
done:

    WdfRequestComplete(WdfRequest, status);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlGetLineControl is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_GET_LINE_CONTROL IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlGetLineControl(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;
    ULONG_PTR reqStatusInfo = 0;

    PSERIAL_LINE_CONTROL serialLineControlPtr;
    status = WdfRequestRetrieveOutputBuffer(
        WdfRequest,
        sizeof(SERIAL_LINE_CONTROL),
        reinterpret_cast<PVOID*>(&serialLineControlPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_LINE_CONTROL buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

    //
    // Get current line control setup
    //
    {
        PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
        KIRQL oldIrql = ExAcquireSpinLockShared(&devExtPtr->ConfigLock);

        *serialLineControlPtr =
            devExtPtr->CurrentConfiguration.LineControlSetup;

        ExReleaseSpinLockShared(&devExtPtr->ConfigLock, oldIrql);

    } // Get current line control setup

    status = STATUS_SUCCESS;
    reqStatusInfo = sizeof(SERIAL_LINE_CONTROL);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_GET_LINE_CONTROL: Word %lu, Stop Bits Code %lu, Parity Code %lu, (status = %!STATUS!)",
        serialLineControlPtr->WordLength,
        serialLineControlPtr->StopBits,
        serialLineControlPtr->Parity,
        status
        );

done:

    WdfRequestCompleteWithInformation(WdfRequest, status, reqStatusInfo);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlGetChars is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_GET_CHARS IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_NOT_SUPPORTED;
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlGetChars(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    UNREFERENCED_PARAMETER(WdfDevice);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_GET_CHARS (status = %!STATUS!)",
        STATUS_NOT_SUPPORTED
        );

    WdfRequestComplete(WdfRequest, STATUS_NOT_SUPPORTED);

    return STATUS_NOT_SUPPORTED;
}


//
// Routine Description:
//
//  PL011IoctlClrRts is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_CLR_RTS IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or 
//  STATUS_NOT_SUPPORTED - The SoC does not expose these control lines.
//  STATUS_INVALID_PARAMETER - HW control is enabled.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlClrRts(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    const PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Check if the SoC exposes RTS
    //
    if ((devExtPtr->UartSupportedControlsMask & UARTCR_RTS) == 0) {

        status = STATUS_NOT_SUPPORTED;
        goto done;
    }

    PL011HwUartControl(
        WdfDevice,
        UARTCR_RTS,
        REG_UPDATE_MODE::BITMASK_CLEAR,
        nullptr
        );

done:

    WdfRequestComplete(WdfRequest, status);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_CLR_RTS (status = %!STATUS!)",
        status
        );

    return status;
}


//
// Routine Description:
//
//  PL011IoctlSetRts is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_SET_RTS IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or 
//  STATUS_NOT_SUPPORTED - The SoC does not expose these control lines.
//  STATUS_INVALID_PARAMETER - HW control is enabled.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlSetRts(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    const PL011_DEVICE_EXTENSION* devExtPtr = 
        PL011DeviceGetExtension(WdfDevice);
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Check if the SoC exposes RTS
    //
    if ((devExtPtr->UartSupportedControlsMask & UARTCR_RTS) == 0) {

        status = STATUS_NOT_SUPPORTED;
        goto done;
    }

    PL011HwUartControl(
        WdfDevice,
        UARTCR_RTS,
        REG_UPDATE_MODE::BITMASK_SET,
        nullptr
        );

done:

    WdfRequestComplete(WdfRequest, status);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_SET_RTS (status = %!STATUS!)",
        status
        );

    return status;
}


//
// Routine Description:
//
//  PL011IoctlClrDtr is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_CLR_DTR IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or 
//  STATUS_NOT_SUPPORTED - The SoC does not expose these control lines.
//  STATUS_INVALID_PARAMETER - HW control is enabled.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlClrDtr(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    const PL011_DEVICE_EXTENSION* devExtPtr = 
        PL011DeviceGetExtension(WdfDevice);
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Check if the SoC exposes RTS
    //
    if ((devExtPtr->UartSupportedControlsMask & UARTCR_DTR) == 0) {

        status = STATUS_NOT_SUPPORTED;
        goto done;
    }

    PL011HwUartControl(
        WdfDevice,
        UARTCR_DTR,
        REG_UPDATE_MODE::BITMASK_CLEAR,
        nullptr
        );

done:

    WdfRequestComplete(WdfRequest, status);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_CLR_DTR (status = %!STATUS!)",
        status
        );

    return status;
}


//
// Routine Description:
//
//  PL011IoctlSetDtr is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_SET_DTR IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or 
//  STATUS_NOT_SUPPORTED - The SoC does not expose these control lines.
//  STATUS_INVALID_PARAMETER - HW control is enabled.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlSetDtr(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    const PL011_DEVICE_EXTENSION* devExtPtr = 
        PL011DeviceGetExtension(WdfDevice);
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Check if the SoC exposes RTS
    //
    if ((devExtPtr->UartSupportedControlsMask & UARTCR_DTR) == 0) {

        status = STATUS_NOT_SUPPORTED;
        goto done;
    }

    PL011HwUartControl(
        WdfDevice,
        UARTCR_DTR,
        REG_UPDATE_MODE::BITMASK_SET,
        nullptr
        );

done:

    WdfRequestComplete(WdfRequest, status);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_SET_DTR (status = %!STATUS!)",
        status
        );

    return status;
}


//
// Routine Description:
//
//  PL011IoctlGetDtrRts is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_GET_DTRRTS IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or error related to the request output buffer validity.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlGetDtrRts(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;
    ULONG_PTR reqStatusInfo = 0;

    ULONG* dtrRtsPtr;
    status = WdfRequestRetrieveOutputBuffer(
        WdfRequest,
        sizeof(ULONG),
        reinterpret_cast<PVOID*>(&dtrRtsPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid IOCTL_SERIAL_GET_DTRRTS buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

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

    *dtrRtsPtr = (regUARTCR & UARTCR_RTS) != 0 ? SERIAL_RTS_STATE : 0;
    *dtrRtsPtr |= ((regUARTCR & UARTCR_DTR) != 0 ? SERIAL_DTR_STATE : 0);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_GET_DTRRTS: 0x%01X, (status = %!STATUS!)",
        UCHAR(*dtrRtsPtr),
        status
        );

    status = STATUS_SUCCESS;
    reqStatusInfo = sizeof(ULONG);

done:

    WdfRequestCompleteWithInformation(WdfRequest, status, reqStatusInfo);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlGetProperties is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_GET_PROPERTIES IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlGetProperties(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;
    ULONG_PTR reqStatusInfo = 0;

    PSERIAL_COMMPROP serialCommPropertiesPtr;
    status = WdfRequestRetrieveOutputBuffer(
        WdfRequest,
        sizeof(SERIAL_COMMPROP),
        reinterpret_cast<PVOID*>(&serialCommPropertiesPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_COMMPROP buffer, (status = %!STATUS!)", status
            );
        goto done;
    }
    RtlZeroMemory(serialCommPropertiesPtr, sizeof(SERIAL_COMMPROP));

    const PL011_DEVICE_EXTENSION* devExtPtr = 
        PL011DeviceGetExtension(WdfDevice);

    //
    // Set the comm properties.
    //
    // Temporarily do not support features that,
    // are not exposed on Raspberry Pi2, like modem control.
    //
    // To Do:
    //  Based on devExtPtr->UartSupportedControlsMask find out
    //  if SoC UART supports modem/flow control and configure
    //  the update the comm properties accordingly.
    //

    serialCommPropertiesPtr->PacketLength = sizeof(SERIAL_COMMPROP);
    serialCommPropertiesPtr->PacketVersion = 2;
    serialCommPropertiesPtr->ServiceMask = SERIAL_SP_SERIALCOMM;
    serialCommPropertiesPtr->ProvSubType = SERIAL_SP_UNSPECIFIED;
    serialCommPropertiesPtr->MaxRxQueue = PL011_RX_BUFFER_SIZE_BYTES;
    serialCommPropertiesPtr->MaxTxQueue = PL011_TX_BUFFER_SIZE_BYTES;
    serialCommPropertiesPtr->CurrentTxQueue = PL011TxGetOutQueue(WdfDevice);
    serialCommPropertiesPtr->CurrentRxQueue = PL011RxGetInQueue(WdfDevice);
    serialCommPropertiesPtr->SettableBaud = devExtPtr->SettableBaud;
    serialCommPropertiesPtr->MaxBaud = 
        devExtPtr->CurrentConfiguration.MaxBaudRateBPS;

    serialCommPropertiesPtr->ProvCapabilities =
        SERIAL_PCF_TOTALTIMEOUTS|
        SERIAL_PCF_PARITY_CHECK |
        SERIAL_PCF_INTTIMEOUTS;
    if ((devExtPtr->UartSupportedControlsMask & UARTCR_RTS) != 0) {

        serialCommPropertiesPtr->ProvCapabilities |= SERIAL_PCF_RTSCTS;
    }
    if ((devExtPtr->UartSupportedControlsMask & UARTCR_DTR) != 0) {

        serialCommPropertiesPtr->ProvCapabilities |= SERIAL_PCF_DTRDSR;
    }

    serialCommPropertiesPtr->SettableParams =
        SERIAL_SP_PARITY    |
        SERIAL_SP_BAUD      |
        SERIAL_SP_DATABITS  |
        SERIAL_SP_STOPBITS;
    if ((devExtPtr->UartSupportedControlsMask & (UARTCR_CTSEn | UARTCR_RTSEn))
        != 0) {

        serialCommPropertiesPtr->SettableParams |= SERIAL_SP_HANDSHAKING;
    }

    serialCommPropertiesPtr->SettableData =
        SERIAL_DATABITS_5 |
        SERIAL_DATABITS_6 |
        SERIAL_DATABITS_7 |
        SERIAL_DATABITS_8;

    serialCommPropertiesPtr->SettableStopParity =
        SERIAL_STOPBITS_10  |
        SERIAL_STOPBITS_20  |
        SERIAL_PARITY_NONE  |
        SERIAL_PARITY_ODD   |
        SERIAL_PARITY_EVEN  |
        SERIAL_PARITY_MARK  |
        SERIAL_PARITY_SPACE;

    status = STATUS_SUCCESS;
    reqStatusInfo = sizeof(SERIAL_COMMPROP);

done:

    WdfRequestCompleteWithInformation(WdfRequest, status, reqStatusInfo);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_GET_PROPERTIES, (status = %!STATUS!)",
        status
        );

    return status;
}


//
// Routine Description:
//
//  PL011IoctlSetBreakOff is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_SET_BREAK_OFF IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlSetBreakOff(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    PL011HwSetBreak(WdfDevice, FALSE);

    WdfRequestComplete(WdfRequest, STATUS_SUCCESS);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_SET_BREAK_OFF, (status = %!STATUS!)",
        STATUS_SUCCESS
        );

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011IoctlGetBaudRate is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_SET_BREAK_ON IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlSetBreakOn(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    PL011HwSetBreak(WdfDevice, TRUE);

    WdfRequestComplete(WdfRequest, STATUS_SUCCESS);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_SET_BREAK_ON, (status = %!STATUS!)",
        STATUS_SUCCESS
        );

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011IoctlGetCommStatus is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_GET_COMMSTATUS IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlGetCommStatus(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;
    ULONG_PTR reqStatusInfo = 0;

    PSERIAL_STATUS serialStatusPtr;
    status = WdfRequestRetrieveOutputBuffer(
        WdfRequest,
        sizeof(SERIAL_STATUS),
        reinterpret_cast<PVOID*>(&serialStatusPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_STATUS buffer, (status = %!STATUS!)", status
            );
        goto done;
    }
    RtlZeroMemory(serialStatusPtr, sizeof(SERIAL_STATUS));

    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);

    serialStatusPtr->AmountInInQueue = PL011RxGetInQueue(WdfDevice);
    serialStatusPtr->AmountInOutQueue = PL011TxGetOutQueue(WdfDevice);
    serialStatusPtr->Errors = InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&devExtPtr->UartErrorTypes),
        0
        );

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_GET_COMMSTATUS: Error types 0x%08X, InQ %lu, OutQ %lu, (status = %!STATUS!)",
        serialStatusPtr->Errors,
        serialStatusPtr->AmountInInQueue,
        serialStatusPtr->AmountInOutQueue,
        status
        );

    status = STATUS_SUCCESS;
    reqStatusInfo = sizeof(SERIAL_STATUS);

done:

    WdfRequestCompleteWithInformation(WdfRequest, status, reqStatusInfo);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlGetModemStatus is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_GET_MODEMSTATUS IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlGetModemStatus(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;
    ULONG_PTR reqStatusInfo = 0;
    ULONG modemStatus = 0;

    UNREFERENCED_PARAMETER(WdfDevice);

    const PL011_DEVICE_EXTENSION* devExtPtr = 
        PL011DeviceGetExtension(WdfDevice);

    if ((devExtPtr->UartSupportedControlsMask &
        UART_CONTROL_LINES_MODEM_STATUS) == 0) {

        status = STATUS_NOT_SUPPORTED;
        goto done;
    }

    ULONG* modemStatusPtr;
    status = WdfRequestRetrieveOutputBuffer(
        WdfRequest,
        sizeof(ULONG),
        reinterpret_cast<PVOID*>(&modemStatusPtr),
        NULL
        );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid SERIAL_STATUS buffer, (status = %!STATUS!)", status
            );
        goto done;
    }

    //
    // Get current UART control
    // and translate PL011 to 16550.
    //

    ULONG regUARTCR = 0;
    PL011HwUartControl(
        WdfDevice,
        0,
        REG_UPDATE_MODE::QUERY,
        &regUARTCR
        );


    if ((regUARTCR & UARTCR_RTS) != 0) {

        modemStatus |= SERIAL_MCR_RTS;
    }

    if ((regUARTCR & UARTCR_DTR) != 0) {

        modemStatus |= SERIAL_MCR_DTR;
    }

    if ((regUARTCR & UARTCR_Out1) != 0) {

        modemStatus |= SERIAL_MCR_OUT1;
    }

    if ((regUARTCR & UARTCR_Out2) != 0) {

        modemStatus |= SERIAL_MCR_OUT2;
    }

    *modemStatusPtr = modemStatus;

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_GET_MODEMSTATUS: Modem status 0x%01X, (status = %!STATUS!)",
        UCHAR(modemStatus),
        status
        );

    status = STATUS_SUCCESS;
    reqStatusInfo = sizeof(ULONG);

done:

    WdfRequestCompleteWithInformation(WdfRequest, status, reqStatusInfo);

    return status;
}


//
// Routine Description:
//
//  PL011IoctlSetFifoControl is called by PL011EvtSerCx2Control to
//  handle IOCTL_SERIAL_SET_FIFO_CONTROL IO control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011IoctlSetFifoControl(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest
    )
{
    NTSTATUS status;

    ULONG fifoControl;
    status = WdfRequestRetrieveInputBuffer(
        WdfRequest,
        sizeof(ULONG),
        reinterpret_cast<PVOID*>(&fifoControl),
        NULL
        );

    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "Invalid IOCTL_SERIAL_SET_FIFO_CONTROL buffer, "
            "(status = %!STATUS!)",
            status
            );
        goto done;
    }

    if ((fifoControl & SERIAL_FCR_DMA_MODE) != 0) {

        status = STATUS_NOT_SUPPORTED;
        PL011_LOG_ERROR(
            "IOCTL_SERIAL_SET_FIFO_CONTROL: "
            "Manually enabling DMA is not supported"
            "(status = %!STATUS!)",
            status
            );
        goto done;
    }

    BOOLEAN isFifoOn = (fifoControl & SERIAL_FCR_ENABLE) != 0;

    //
    // Select TX FIFO level
    //
    UARTIFLS_TXIFLSEL txFifoLevel = UARTIFLS_TXIFLSEL::TXIFLSEL_1_8;
    switch (fifoControl & SERIAL_TX_FIFO_MASK) {

    case SERIAL_TX_1_BYTE_TRIG:
        txFifoLevel = UARTIFLS_TXIFLSEL::TXIFLSEL_1_8;
        break;

    case SERIAL_TX_4_BYTE_TRIG:
        txFifoLevel = UARTIFLS_TXIFLSEL::TXIFLSEL_1_4;
        break;

    case SERIAL_TX_8_BYTE_TRIG:
        txFifoLevel = UARTIFLS_TXIFLSEL::TXIFLSEL_1_2;
        break;

    case SERIAL_TX_14_BYTE_TRIG:
        txFifoLevel = UARTIFLS_TXIFLSEL::TXIFLSEL_7_8;
        break;

    } // switch TX FIFO

    //
    // Select RX FIFO level
    //
    UARTIFLS_RXIFLSEL rxFifoLevel = UARTIFLS_RXIFLSEL::RXIFLSEL_7_8;
    switch (fifoControl & SERIAL_RX_FIFO_MASK) {

    case SERIAL_1_BYTE_HIGH_WATER:
        rxFifoLevel = UARTIFLS_RXIFLSEL::RXIFLSEL_1_8;
        break;

    case SERIAL_4_BYTE_HIGH_WATER:
        rxFifoLevel = UARTIFLS_RXIFLSEL::RXIFLSEL_1_4;
        break;

    case SERIAL_8_BYTE_HIGH_WATER:
        rxFifoLevel = UARTIFLS_RXIFLSEL::RXIFLSEL_1_2;
        break;

    case SERIAL_14_BYTE_HIGH_WATER:
        rxFifoLevel = UARTIFLS_RXIFLSEL::RXIFLSEL_7_8;
        break;

    } // switch TX FIFO

    if ((fifoControl & SERIAL_FCR_RCVR_RESET) != 0) {

        PL011RxPurgeFifo(WdfDevice, nullptr);
    }

    if ((fifoControl & SERIAL_FCR_TXMT_RESET) != 0) {

        PL011TxPurgeFifo(WdfDevice, nullptr);
    }

    PL011HwSetFifoThreshold(WdfDevice, rxFifoLevel, txFifoLevel);

    PL011HwEnableFifos(WdfDevice, isFifoOn);

    PL011_LOG_INFORMATION(
        "IOCTL_SERIAL_SET_FIFO_CONTROL, (status = %!STATUS!)",
        status
        );

    status = STATUS_SUCCESS;

done:

    WdfRequestComplete(WdfRequest, status);

    return status;
}


#undef _PL011_IOCTL_CPP_