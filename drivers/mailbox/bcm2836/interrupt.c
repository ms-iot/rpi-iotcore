//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     Interrupt.c
//
// Abstract:
//
//     This file contains the interrupt related implementation.
//

#include "precomp.h"

#include "trace.h"
#include "interrupt.tmh"

#include "register.h"
#include "device.h"
#include "interrupt.h"
#include "mailbox.h"

RPIQ_PAGED_SEGMENT_BEGIN

/*++

Routine Description:

    This function initialized the mailbox interrupt.

Arguments:

    DeviceContextPtr - Pointer to device context

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS RpiqEnableInterrupts (
    DEVICE_CONTEXT* DeviceContextPtr
    )
{
    ULONG reg;

    PAGED_CODE();

    // Only enable data available interrupt.
    reg = READ_REGISTER_NOFENCE_ULONG(&DeviceContextPtr->Mailbox->Config);
    reg |= MAILBOX_DATA_AVAIL_ENABLE_IRQ;
    WRITE_REGISTER_NOFENCE_ULONG(&DeviceContextPtr->Mailbox->Config, reg);

    return STATUS_SUCCESS;
}

/*++

Routine Description:

    This function disable all interrupts.

Arguments:

    DeviceContextPtr - Pointer to device context

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS RpiqDisableInterrupts (
    DEVICE_CONTEXT* DeviceContextPtr
    )
{
    ULONG reg;

    PAGED_CODE();

    // Disable all interrupt.
    reg = READ_REGISTER_NOFENCE_ULONG(&DeviceContextPtr->Mailbox->Config);
    reg &= ~MAILBOX_MASK_IRQ;
    WRITE_REGISTER_NOFENCE_ULONG(&DeviceContextPtr->Mailbox->Config, reg);

    return STATUS_SUCCESS;
}

RPIQ_PAGED_SEGMENT_END

RPIQ_NONPAGED_SEGMENT_BEGIN

/*++

Routine Description:

    Mailbox ISR handler.

Arguments:

Interrupt - A handle to a framework interrupt object.

    MessageID - If the device is using message-signaled interrupts (MSIs),
        this parameter is the message number that identifies the device's
        hardware interrupt message.

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
BOOLEAN RpiqMailboxIsr (
    WDFINTERRUPT Interrupt,
    ULONG MessageID
    )
{
    DEVICE_CONTEXT *deviceContextPtr;
    WDFDEVICE device = WdfInterruptGetDevice(Interrupt);  
    BOOLEAN claimInterrupt = FALSE;
    ULONG reg;

    UNREFERENCED_PARAMETER(MessageID);

    deviceContextPtr = RpiqGetContext(device);

    reg = READ_REGISTER_NOFENCE_ULONG(&deviceContextPtr->Mailbox->Config);

    if((reg & MAILBOX_DATA_AVAIL_PENDING) != 0) {
        // Disable interrupt and let DPC handle all incoming data from 
        // mailbox. DPC would be responsible to process all mailbox content and
        // once it is empty would re-enable the interrupt again.
        reg = READ_REGISTER_NOFENCE_ULONG(&deviceContextPtr->Mailbox->Config);
        reg &= ~MAILBOX_DATA_AVAIL_ENABLE_IRQ;
        WRITE_REGISTER_NOFENCE_ULONG(&deviceContextPtr->Mailbox->Config, reg);

        claimInterrupt = TRUE;
        WdfInterruptQueueDpcForIsr(deviceContextPtr->MailboxIntObj);
    }

    RPIQ_LOG_INFORMATION(
        "Mailbox isr claimInterrupt %d", 
        claimInterrupt);

    return claimInterrupt;
}

/*++

Routine Description:

    Mailbox DPC Interrupt handler.

Arguments:

    Interrupt - A handle to a framework interrupt object.

    AssociatedObject - A handle to the framework device object that the driver
        passed to WdfInterruptCreate.

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
VOID RpiqMailboxDpc (
    WDFINTERRUPT Interrupt,
    WDFOBJECT AssociatedObject
    )
{
    DEVICE_CONTEXT* deviceContextPtr;
    ULONG value, channel, reg;

    UNREFERENCED_PARAMETER(AssociatedObject);

    deviceContextPtr = RpiqGetContext(WdfInterruptGetDevice(Interrupt));

    reg = READ_REGISTER_NOFENCE_ULONG(&deviceContextPtr->Mailbox->Status);

    while (!(reg & MAILBOX_STATUS_EMPTY)) {
        value = READ_REGISTER_NOFENCE_ULONG(&deviceContextPtr->Mailbox->Read);
        reg = READ_REGISTER_NOFENCE_ULONG(&deviceContextPtr->Mailbox->Status);
        channel = value & MAILBOX_CHANNEL_MASK;

        if (channel >= MAILBOX_CHANNEL_MAX) {
            RPIQ_LOG_WARNING("Unknown mailbox message channel");
            continue;
        }

        WDFREQUEST nextRequest;
        NTSTATUS status = WdfIoQueueRetrieveNextRequest(
            deviceContextPtr->ChannelQueue[channel],
            &nextRequest);
        if (!NT_SUCCESS(status)) {
            RPIQ_LOG_ERROR(
                "WdfIoQueueRetrieveNextRequest failed  %!STATUS!",
                status);
            continue;
        }

        RPIQ_REQUEST_CONTEXT* requestContextPtr = 
            RpiqGetRequestContext(nextRequest);
        MAILBOX_HEADER* outputBufferPtr;

        status = WdfRequestRetrieveOutputBuffer(
            nextRequest,
            requestContextPtr->PropertyMemorySize,
            &outputBufferPtr,
            NULL);
        if (!NT_SUCCESS(status)) {
            RPIQ_LOG_ERROR(
                "WdfRequestRetrieveOutputBuffer failed %!STATUS!",
                status);
            WdfRequestComplete(nextRequest, status);
            continue;
        }

        RtlCopyMemory(
            outputBufferPtr,
            requestContextPtr->PropertyMemory, 
            requestContextPtr->PropertyMemorySize);

        WdfRequestCompleteWithInformation(
            nextRequest, 
            STATUS_SUCCESS, 
            requestContextPtr->PropertyMemorySize);
    }

    WdfInterruptAcquireLock(Interrupt);

    // Enable interrupt again
    reg = READ_REGISTER_NOFENCE_ULONG(&deviceContextPtr->Mailbox->Config);
    reg |= MAILBOX_DATA_AVAIL_ENABLE_IRQ;
    WRITE_REGISTER_NOFENCE_ULONG(&deviceContextPtr->Mailbox->Config, reg);

    WdfInterruptReleaseLock(Interrupt);
}

RPIQ_NONPAGED_SEGMENT_END