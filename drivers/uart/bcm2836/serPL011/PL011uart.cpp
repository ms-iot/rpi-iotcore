//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    PL011uart.cpp
//
// Abstract:
//
//    This module contains implementation of SerCx2 general event callbacks
//    used by the ARM PL011 device driver.
//    More SerCX2 event handlers reside in the following modules:
//      - PL011rx
//      - PL011tx
//      - PL011interrupt
//
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//
#include "precomp.h"
#pragma hdrstop

#define _PL011_UART_CPP_

// Logging header files
#include "PL011logging.h"
#include "PL011uart.tmh"

// Common driver header files
#include "PL011common.h"

// Module specific header files
#include "PL011uart.h"
#include "PL011ioctl.h"
#include "PL011rx.h"
#include "PL011tx.h"


#ifdef ALLOC_PRAGMA
    #pragma alloc_text(PAGE, PL011EvtSerCx2ApplyConfig)
    #pragma alloc_text(PAGE, PL011EvtSerCx2PurgeFifos)
    #pragma alloc_text(PAGE, PL011EvtSerCx2ApplyConfig)

    #pragma alloc_text(PAGE, PL011pParseSerialBusDescriptor)
#endif


//
// Routine Description:
//
//  PL011EvtSerCx2ApplyConfig is called by SerCx2 framework to apply a device
//  specific default configuration (from UEFI) settings to the controller.
//  The routine gets the connection parameters from 'resource hub' extracts
//  the PL011 specific information, and applies them to the controller calling
//  PL011pApplyConfig().
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  ConnectionParametersPtr - A connection parameters from resource hub, that
//      contains the device specific configuration.
//
// Return Value:
//
//  STATUS_SUCCESS, if parameters we successfully parsed and applied to
//      controller.
//
_Use_decl_annotations_
NTSTATUS
PL011EvtSerCx2ApplyConfig(
    WDFDEVICE WdfDevice,
    PVOID ConnectionParametersPtr
    )
{
    PAGED_CODE();

    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);

    RtlZeroMemory(
        &devExtPtr->CurrentConfiguration.UartSerialBusDescriptor,
        sizeof(PNP_UART_SERIAL_BUS_DESCRIPTOR)
        );

    //
    // Parse the descriptor we got from ACPI
    //
    PPNP_UART_SERIAL_BUS_DESCRIPTOR pnpUartDescriptorPtr;
    NTSTATUS status = PL011pParseSerialBusDescriptor(
                            WdfDevice,
                            ConnectionParametersPtr,
                            &pnpUartDescriptorPtr
                            );
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "PL011GetSerialBusDescriptor failed, (status = %!STATUS!)",
            status
            );
        return status;
    }

    PL011_LOG_INFORMATION(
        "UART Connection Descriptor %p, Baud:%lu, RxBufferSize:%u, TxBufferSize:%u, Parity:%x, Flags:%hx",
        pnpUartDescriptorPtr,
        pnpUartDescriptorPtr->BaudRate,
        pnpUartDescriptorPtr->RxBufferSize,
        pnpUartDescriptorPtr->TxBufferSize,
        pnpUartDescriptorPtr->Parity,
        pnpUartDescriptorPtr->SerialBusDescriptor.TypeSpecificFlags
        );

    //
    // Apply default configuration
    //
    status = PL011pApplyConfig(WdfDevice, pnpUartDescriptorPtr);
    if (!NT_SUCCESS(status)) {

        return status;
    }

    return status;
}


//
// Routine Description:
//
//  PL011EvtSerCx2Control is called by SerCx2 framework to handle IO
//  control requests.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The WDF object that represent the IO control request.
//
//  OutputBufferLength - The request output buffer length in bytes.
//
//  InputBufferLength - The request input buffer length in bytes.
//
//  IoControlCode - The request IO control code
//
// Return Value:
//
//  STATUS_SUCCESS, or appropriate error code.
//
_Use_decl_annotations_
NTSTATUS
PL011EvtSerCx2Control(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
    )
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    switch (IoControlCode) {

    case IOCTL_SERIAL_SET_BAUD_RATE:
        status = PL011IoctlSetBaudRate(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_GET_BAUD_RATE:
        status = PL011IoctlGetBaudRate(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_SET_HANDFLOW:
        status = PL011IoctlSetHandflow(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_GET_HANDFLOW:
        status = PL011IoctlGetHandflow(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_SET_MODEM_CONTROL:
        status = PL011IoctlSetModemControl(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_GET_MODEM_CONTROL:
        status = PL011IoctlGetModemControl(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_SET_LINE_CONTROL:
        status = PL011IoctlSetLineControl(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_GET_LINE_CONTROL:
        status = PL011IoctlGetLineControl(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_GET_CHARS:
        status = PL011IoctlGetChars(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_CLR_RTS:
        status = PL011IoctlClrRts(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_SET_RTS:
        status = PL011IoctlSetRts(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_CLR_DTR:
        status = PL011IoctlClrDtr(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_SET_DTR:
        status = PL011IoctlSetDtr(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_GET_DTRRTS:
        status = PL011IoctlGetDtrRts(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_GET_PROPERTIES:
        status = PL011IoctlGetProperties(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_SET_BREAK_OFF:
        status = PL011IoctlSetBreakOff(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_SET_BREAK_ON:
        status = PL011IoctlSetBreakOn(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_GET_COMMSTATUS:
        status = PL011IoctlGetCommStatus(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_GET_MODEMSTATUS:
        status = PL011IoctlGetModemStatus(WdfDevice, WdfRequest);
        break;

    case IOCTL_SERIAL_SET_FIFO_CONTROL:
        status = PL011IoctlSetFifoControl(WdfDevice, WdfRequest);
        break;

    default:
        status = STATUS_NOT_SUPPORTED;
        PL011_LOG_ERROR(
            "IO control code not supported 0x%08X",
            IoControlCode
            );
        WdfRequestComplete(WdfRequest, status);
        break;

    } // switch (IoControlCode)

    return status;
}


//
// Routine Description:
//
//  PL011EvtSerCx2PurgeFifos is called by SerCx2 framework to purge the
//  FIFO buffers in the PL011 serial controller hardware.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  IsPurgeRxFifo - Whether to purge the receive FIFO.
//
//  IsPurgeTxFifo - Whether to purge the transmit FIFO
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011EvtSerCx2PurgeFifos(
    WDFDEVICE WdfDevice,
    BOOLEAN IsPurgeRxFifo,
    BOOLEAN IsPurgeTxFifo
    )
{
    PAGED_CODE();

    if (IsPurgeRxFifo) {

        PL011RxPurgeFifo(WdfDevice, nullptr);
    }

    if (IsPurgeTxFifo) {

        PL011TxPurgeFifo(WdfDevice, nullptr);
    }
}


//
// Routine Description:
//
//  PL011EvtSerCx2SetWaitMask is called by SerCx2 framework to handle
//  IOCTL_SERIAL_SET_WAIT_MASK requests and configure the serial controller
//  to monitor a set of hardware events that are specified by a wait mask.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  WdfRequest - The IOCTL_SERIAL_SET_WAIT_MASK request.
//
//  WaitMask - The SERIAL_EV_XXX bit mask for the events the device driver
//      should monitor
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011EvtSerCx2SetWaitMask(
    WDFDEVICE WdfDevice,
    WDFREQUEST WdfRequest,
    ULONG WaitMask
    )
{
    NTSTATUS status;
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);

    //
    // The supported event mask.
    // SERIAL_EV_RXFLAG and SERIAL_EV_TXEMPTY are
    // handled through SerCx2 RX/TX interface.
    //
    // Temporarily do not support modem control,
    // since modem control signals are not exposed on
    // Raspberry Pi2.
    //
    // To Do:
    //  Based on devExtPtr->UartSupportedControlsMask find out
    //  if SoC UART supports flow control and set
    //  the supported events accordingly.
    //
    ULONG supportedEvents = SERIAL_EV_BREAK |
                            SERIAL_EV_ERR;
    if ((devExtPtr->UartSupportedControlsMask & UARTCR_RTS) != 0) {

        supportedEvents |= SERIAL_EV_CTS;
    }

    if ((devExtPtr->UartSupportedControlsMask & UARTCR_DTR) != 0) {

        supportedEvents |= SERIAL_EV_DSR;
    }

    if ((WaitMask & ~supportedEvents) != 0) {

        status = STATUS_NOT_SUPPORTED;
        PL011_LOG_ERROR(
            "Unsupported wait mask 0x%8X",
            WaitMask
            );
        goto done;
    }

    // The events to enable
    ULONG eventsToEnable = 0;

    if ((WaitMask & SERIAL_EV_BREAK) != 0) {

        eventsToEnable |= UARTIMSC_BEIM;
    }

    if ((WaitMask & SERIAL_EV_ERR) != 0) {

        eventsToEnable |= UART_INTERUPPTS_ERRORS;
    }

    if ((WaitMask & SERIAL_EV_CTS) != 0) {

        eventsToEnable |= UARTIMSC_CTSMIM;
    }

    if ((WaitMask & SERIAL_EV_DSR) != 0) {

        eventsToEnable |= UARTIMSC_DSRMIM;
    }

    //
    // Update the interrupt mask
    //
    {
        KIRQL oldIrql = ExAcquireSpinLockExclusive(&devExtPtr->ConfigLock);

        // Mask all events (without RX/TX)
        PL011HwMaskInterrupts(
            WdfDevice,
            UART_INTERRUPTS_EVENTS,
            TRUE, // mask
            TRUE // ISR safe
            );

        // Unmask required interrupts
        PL011HwMaskInterrupts(
            WdfDevice,
            eventsToEnable,
            FALSE, // Unmask
            TRUE   // ISR safe
            );

        devExtPtr->WaitEventMask = WaitMask;

        ExReleaseSpinLockExclusive(&devExtPtr->ConfigLock, oldIrql);

    } // Update the interrupt mask

    status = STATUS_SUCCESS;

done:

    WdfRequestComplete(WdfRequest, status);
}


//
// Routine Description:
//
//  PL011EvtSerCx2FileOpen is called by SerCx2 to notify
//  the serial controller driver that a client opened a logical connection to
//  the serial controller.
//  The routine initializes the controller on first open instance.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
// Return Value:
//
//  STATUS_SUCCESS,
//  STATUS_DEVICE_NOT_READY - if we have a conflict with the debugger,
//  or controller initialization status on first open.
//
_Use_decl_annotations_
NTSTATUS
PL011EvtSerCx2FileOpen(
    WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);

    if (devExtPtr->IsDebuggerConflict) {

        PL011_LOG_ASSERTION(
            "A FileOpen request should never be received when a debugger conflict "
            "is detected. If no FunctionConfig() resource was supplied, the driver "
            "should have failed to load. If an FunctionConfig() resource was "
            "supplied, muxing arbitration should prevent a FileOpen() request "
            "from ever reaching the driver."
            );
        return STATUS_DEVICE_NOT_READY;
    }

    NTSTATUS status;
    KLOCK_QUEUE_HANDLE lockHandle;
    KeAcquireInStackQueuedSpinLock(&devExtPtr->Lock, &lockHandle);

    if ((++devExtPtr->OpenCount) == 1) {

        status = PL011HwInitController(WdfDevice);
        if (!NT_SUCCESS(status)) {

            PL011_LOG_ERROR(
                "PL011HwInitController failed, (status = %!STATUS!)", status
                );
            goto done;
        }

        status = PL011RxPioReceiveStart(WdfDevice);
        if (!NT_SUCCESS(status)) {

            PL011HwStopController(WdfDevice);

            PL011_LOG_ERROR(
                "PL011RxPioReceiveStart failed, (status = %!STATUS!)", status
                );
            goto done;
        }

        status = PL011TxPioTransmitStart(WdfDevice);
        if (!NT_SUCCESS(status)) {

            (void)PL011RxPioReceiveStop(WdfDevice);
            PL011HwStopController(WdfDevice);

            PL011_LOG_ERROR(
                "PL011TxPioTransmitStart failed, (status = %!STATUS!)", status
                );
            goto done;
        }

    } // First open()

    status = STATUS_SUCCESS;

done:

    KeReleaseInStackQueuedSpinLock(&lockHandle);

    return status;
}


//
// Routine Description:
//
//  PL011EvtSerCx2FileClose is called by SerCx2 to notify
//  the serial controller driver that a client released the file object
//  that represents the logical connection to the serial controller device.
//  The routine stops the controller on last open instance.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
// Return Value:
//
//  STATUS_SUCCESS, or controller initialization status on first open.
//
_Use_decl_annotations_
VOID
PL011EvtSerCx2FileClose(
    WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);

    PL011_ASSERT(!devExtPtr->IsDebuggerConflict);

    KLOCK_QUEUE_HANDLE lockHandle;
    KeAcquireInStackQueuedSpinLock(&devExtPtr->Lock, &lockHandle);

    if ((--devExtPtr->OpenCount) == 0) {

        (void)PL011RxPioReceiveStop(WdfDevice);
        (void)PL011TxPioTransmitStop(WdfDevice);

        PL011HwStopController(WdfDevice);

    } // Last close()

    PL011_ASSERT(devExtPtr->OpenCount >= 0);

    KeReleaseInStackQueuedSpinLock(&lockHandle);
}


//
// Routine Description:
//
//  PL011pParseSerialBusDescriptor is called by PL011EvtSerCx2ApplyConfig
//  to parse the connection information buffer.
//  The routine verifies the parameters buffer validity and return a reference
//  to the configuration parameters buffer embedded within the connection
//  information buffer.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  ConnectionParametersPtr - The connection information buffer.
//
//  PnPUartDescriptorPPtr - Address of a caller PNP_UART_SERIAL_BUS_DESCRIPTOR
//      var to receive the address of the default configuration parameters.
//
// Return Value:
//
//  STATUS_SUCCESS, or STATUS_INVALID_PARAMETER if the connection information
//      buffer is invalid.
//
_Use_decl_annotations_
NTSTATUS
PL011pParseSerialBusDescriptor(
    WDFDEVICE WdfDevice,
    VOID* ConnectionParametersPtr,
    PNP_UART_SERIAL_BUS_DESCRIPTOR** PnPUartDescriptorPPtr
    )
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(WdfDevice);

    //
    // Default return value
    //
    PL011_ASSERT(PnPUartDescriptorPPtr != nullptr);
    *PnPUartDescriptorPPtr = nullptr;

    if (ConnectionParametersPtr == nullptr) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    // Validate and parse connection information
    //

    PRH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER connectionPtr =
        static_cast<PRH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER>(
            ConnectionParametersPtr
            );

    if (connectionPtr->PropertiesLength < sizeof(PNP_UART_SERIAL_BUS_DESCRIPTOR)) {

        PL011_LOG_ERROR(
            "Invalid connection properties (length = %lu, expected = %lu)",
            connectionPtr->PropertiesLength,
            sizeof(PPNP_SERIAL_BUS_DESCRIPTOR)
            );
        return STATUS_INVALID_PARAMETER;
    }

    const PNP_SERIAL_BUS_DESCRIPTOR* serialBusDescriptorPtr =
        reinterpret_cast<const PNP_SERIAL_BUS_DESCRIPTOR*>(
            &connectionPtr->ConnectionProperties[0]
            );

    if (serialBusDescriptorPtr->SerialBusType != UART_SERIAL_BUS_TYPE) {

        PL011_LOG_ERROR(
            "Bus type %c not supported, only UART (%lu) is supported",
            serialBusDescriptorPtr->SerialBusType,
            UART_SERIAL_BUS_TYPE
            );
        return STATUS_INVALID_PARAMETER;
    }

    *PnPUartDescriptorPPtr = reinterpret_cast<PPNP_UART_SERIAL_BUS_DESCRIPTOR>(
        &connectionPtr->ConnectionProperties[0]
        );

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011pApplyConfig is called by PL011EvtSerCx2ApplyConfig
//  to apply the given configuration to the controller.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  ConnectionParametersPtr - The connection information buffer.
//
//  PnPUartDescriptorPtr - The UART descriptor that contains the
//      var the default configuration parameters.
//
// Return Value:
//
//  STATUS_SUCCESS
//  STATUS_INVALID_PARAMETER - At least one of the configuration parameters
//      is invalid.
//  STATUS_NOT_SUPPORTED - At least one of the configuration parameters
//      is not supported.
//  STATUS_NOT_IMPLEMENTED - At least one of the configuration parameters
//      is not implemented
//
_Use_decl_annotations_
NTSTATUS
PL011pApplyConfig(
    WDFDEVICE WdfDevice,
    PNP_UART_SERIAL_BUS_DESCRIPTOR* PnpUartDescriptorPtr
    )
{
    NTSTATUS status;
    const PNP_SERIAL_BUS_DESCRIPTOR* serialBusDescPtr =
        &PnpUartDescriptorPtr->SerialBusDescriptor;

    //
    // Configure baud rate
    //

    status = PL011HwSetBaudRate(WdfDevice, PnpUartDescriptorPtr->BaudRate);
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "PL011HwSetBaud failed, (status = %!STATUS!)", status
            );
        return status;
    }

    //
    // Get the flow control setup
    //
    SERIAL_HANDFLOW flowControlSetup = { 0 };
    {
        USHORT uartFlowControlParams =
            serialBusDescPtr->TypeSpecificFlags & UART_SERIAL_FLAG_FLOW_CTL_MASK;

        switch (uartFlowControlParams) {

        case UART_SERIAL_FLAG_FLOW_CTL_NONE:
        {
            if ((PnpUartDescriptorPtr->SerialLinesEnabled &
                UART_SERIAL_LINES_RTS) != 0) {

                flowControlSetup.FlowReplace |= SERIAL_RTS_CONTROL;
            }

            if ((PnpUartDescriptorPtr->SerialLinesEnabled &
                UART_SERIAL_LINES_DTR) != 0) {

                flowControlSetup.ControlHandShake |= SERIAL_DTR_CONTROL;
            }
            break;

        } // UART_SERIAL_FLAG_FLOW_CTL_NONE

        case UART_SERIAL_FLAG_FLOW_CTL_XONXOFF:
        {
            PL011_LOG_ERROR(
                "Software flow control is not implemented, (status = %!STATUS!)", STATUS_NOT_IMPLEMENTED
                );
            return STATUS_NOT_IMPLEMENTED;

        } // UART_SERIAL_FLAG_FLOW_CTL_XONXOFF

        case UART_SERIAL_FLAG_FLOW_CTL_HW:
        {
            if ((PnpUartDescriptorPtr->SerialLinesEnabled &
                UART_SERIAL_LINES_RTS) != 0) {

                flowControlSetup.FlowReplace |= SERIAL_RTS_HANDSHAKE;
            }

            if ((PnpUartDescriptorPtr->SerialLinesEnabled &
                UART_SERIAL_LINES_CTS) != 0) {

                flowControlSetup.ControlHandShake |= SERIAL_CTS_HANDSHAKE;
            }
            break;

        } // UART_SERIAL_FLAG_FLOW_CTL_HW

        default:
            PL011_LOG_ERROR(
                "Unsupported flow control setup parameter 0x%04X, (status = %!STATUS!)",
                uartFlowControlParams,
                STATUS_NOT_SUPPORTED
                );
            return STATUS_NOT_SUPPORTED;

        } // uartFlowControlMask

    } // Configure flow control

    //
    // Get line control setup
    //
    SERIAL_LINE_CONTROL lineControlSetup = { 0 };
    {
        //
        // Word size
        //
        USHORT lineControlParams =
            serialBusDescPtr->TypeSpecificFlags & UART_SERIAL_FLAG_DATA_BITS_MASK;

        switch (lineControlParams) {

        case UART_SERIAL_FLAG_DATA_BITS_5:
            lineControlSetup.WordLength = 5;
            break;

        case UART_SERIAL_FLAG_DATA_BITS_6:
            lineControlSetup.WordLength = 6;
            break;

        case UART_SERIAL_FLAG_DATA_BITS_7:
            lineControlSetup.WordLength = 7;
            break;

        case UART_SERIAL_FLAG_DATA_BITS_8:
            lineControlSetup.WordLength = 8;
            break;

        case UART_SERIAL_FLAG_DATA_BITS_9:
            __fallthrough;
        default:
            PL011_LOG_ERROR(
                "Unsupported word size setup parameter 0x%04X, (status = %!STATUS!)",
                lineControlParams,
                STATUS_NOT_SUPPORTED
                );
            return STATUS_NOT_SUPPORTED;

        } // switch (lineControlParams)

        //
        // Stop bits
        //
        lineControlParams =
            serialBusDescPtr->TypeSpecificFlags & UART_SERIAL_FLAG_STOP_BITS_MASK;

        switch (lineControlParams) {

        case UART_SERIAL_FLAG_STOP_BITS_1:
            lineControlSetup.StopBits = STOP_BIT_1;
            break;

        case UART_SERIAL_FLAG_STOP_BITS_2:
            lineControlSetup.StopBits = STOP_BITS_2;
            break;

        case UART_SERIAL_FLAG_STOP_BITS_1_5:
        case UART_SERIAL_FLAG_STOP_BITS_0:
            __fallthrough;
        default:
            PL011_LOG_ERROR(
                "Unsupported stop bits setup parameter 0x%04X, (status = %!STATUS!)",
                lineControlParams,
                STATUS_NOT_SUPPORTED
                );
            return STATUS_NOT_SUPPORTED;

        } // switch (lineControlParams)

        //
        // Stop bits
        //
        switch (PnpUartDescriptorPtr->Parity) {

        case UART_SERIAL_PARITY_NONE:
            lineControlSetup.Parity = NO_PARITY;
            break;

        case UART_SERIAL_PARITY_ODD:
            lineControlSetup.Parity = ODD_PARITY;
            break;

        case UART_SERIAL_PARITY_EVEN:
            lineControlSetup.Parity = EVEN_PARITY;
            break;

        case UART_SERIAL_PARITY_MARK:
            lineControlSetup.Parity = MARK_PARITY;
            break;

        case UART_SERIAL_PARITY_SPACE:
            lineControlSetup.Parity = SPACE_PARITY;
            break;

        default:
            PL011_LOG_ERROR(
                "Unsupported parity setup parameter 0x%04X, (status = %!STATUS!)",
                lineControlParams,
                STATUS_NOT_SUPPORTED
                );
            return STATUS_NOT_SUPPORTED;

        } // switch (PnpUartDescriptorPtr->Parity)

    } // Get line control setup

    //
    // Apply line control setup
    //
    status = PL011HwSetFlowControl(WdfDevice, &flowControlSetup);
    if (!NT_SUCCESS(status))
    {
        PL011_LOG_ERROR(
            "PL011HwFlowControl failed, (status = %!STATUS!)", status
            );
        return status;
    }

    //
    // Apply line control setup
    //
    status = PL011HwSetLineControl(WdfDevice, &lineControlSetup);
    if (!NT_SUCCESS(status)) {

        PL011_LOG_ERROR(
            "PL011HwSetLineControl failed, (status = %!STATUS!)", status
            );
        return status;
    }

    //
    // Enable RX/TX FIFOs
    //
    PL011HwEnableFifos(WdfDevice, TRUE);

    //
    // Update the current configuration
    //
    {
        PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
        KIRQL oldIrql = ExAcquireSpinLockExclusive(&devExtPtr->ConfigLock);

        RtlCopyMemory(
            &devExtPtr->CurrentConfiguration.UartSerialBusDescriptor,
            PnpUartDescriptorPtr,
            sizeof(PNP_UART_SERIAL_BUS_DESCRIPTOR)
            );

        devExtPtr->CurrentConfiguration.FlowControlSetup = flowControlSetup;
        devExtPtr->CurrentConfiguration.LineControlSetup = lineControlSetup;

        ExReleaseSpinLockExclusive(&devExtPtr->ConfigLock, oldIrql);

    } // Update the current configuration

    return STATUS_SUCCESS;
}


#undef _PL011_UART_CPP_