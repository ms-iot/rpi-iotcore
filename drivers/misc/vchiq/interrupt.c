//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//      Interrupt.c
//
// Abstract:
//
//      This file contains the interrupt related implementation.
//

#include "precomp.h"

#include "trace.h"
#include "interrupt.tmh"

#include "slotscommon.h"
#include "device.h"
#include "file.h"
#include "slots.h"
#include "interrupt.h"

VCHIQ_PAGED_SEGMENT_BEGIN

/*++

Routine Description:

     This function initialized the VCHIQ interrupt.

Arguments:

     DeviceContextPtr - Pointer to device context

Return Value:

     NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS
VchiqEnableInterrupts (
    DEVICE_CONTEXT* DeviceContextPtr
    )
{
    UNREFERENCED_PARAMETER(DeviceContextPtr);

    PAGED_CODE();

    // Place holder function for future interrupt enabling

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
NTSTATUS
VchiqDisableInterrupts (
    DEVICE_CONTEXT* DeviceContextPtr
    )
{
    UNREFERENCED_PARAMETER(DeviceContextPtr);

    PAGED_CODE();

    // Place holder function for future interrupt disbaling

    return STATUS_SUCCESS;
}

VCHIQ_PAGED_SEGMENT_END

VCHIQ_NONPAGED_SEGMENT_BEGIN

/*++

Routine Description:

     VCHIQ ISR handlers.

Arguments:

     Interrupt - A handle to a framework interrupt object.

     MessageID - If the device is using message-signaled interrupts (MSIs),
     this parameter is the message number that identifies the device's
     hardware interrupt message.

Return Value:

     NTSTATUS

--*/
_Use_decl_annotations_
BOOLEAN VchiqIsr (
    WDFINTERRUPT Interrupt,
    ULONG MessageID
    )
{
    ULONG vchiqStatus;
    BOOLEAN claimInterrupt = FALSE;
    DEVICE_CONTEXT* deviceContextPtr;

    UNREFERENCED_PARAMETER(MessageID);

    deviceContextPtr = VchiqGetDeviceContext(WdfInterruptGetDevice(Interrupt));

    vchiqStatus =
        READ_REGISTER_NOFENCE_ULONG((ULONG*)(deviceContextPtr->VchiqRegisterPtr + BELL0));

    // Check if VC firmware rang the doorbell
    if (vchiqStatus & 0x4) {
        claimInterrupt = TRUE;
        WdfInterruptQueueDpcForIsr(deviceContextPtr->VchiqIntObj);
    }


    return claimInterrupt;
}

/*++

Routine Description:

     Vchiq DPC Interrupt handler.

Arguments:

     Interrupt - A handle to a framework interrupt object.

     AssociatedObject - A handle to the framework device object that the driver
     passed to WdfInterruptCreate.

Return Value:

     NTSTATUS

--*/
_Use_decl_annotations_
VOID VchiqDpc (
    WDFINTERRUPT Interrupt,
    WDFOBJECT AssociatedObject
    )
{
    DEVICE_CONTEXT* deviceContextPtr;
    VCHIQ_SLOT_ZERO* slotZeroPtr;

    UNREFERENCED_PARAMETER(AssociatedObject);

    deviceContextPtr = VchiqGetDeviceContext(WdfInterruptGetDevice(Interrupt));
    slotZeroPtr = deviceContextPtr->SlotZeroPtr;

    // Process slots based on incoming interrupts
    if (slotZeroPtr->Slave.Trigger.Armed &&
        slotZeroPtr->Slave.Trigger.Fired) {

        slotZeroPtr->Slave.Trigger.Armed = 0;
        KeSetEvent(
            &deviceContextPtr->VchiqThreadEvent[THREAD_TRIGGER], 
            0, 
            FALSE);
    }

    if (slotZeroPtr->Slave.Recycle.Armed &&
        slotZeroPtr->Slave.Recycle.Fired) {

        slotZeroPtr->Slave.Recycle.Armed = 0;
        KeSetEvent(
            &deviceContextPtr->VchiqThreadEvent[THREAD_RECYCLE],
            0,
            FALSE);
    }

    if (slotZeroPtr->Slave.SyncTrigger.Armed &&
        slotZeroPtr->Slave.SyncTrigger.Fired) {

        slotZeroPtr->Slave.SyncTrigger.Armed = 0;
        KeSetEvent(
            &deviceContextPtr->VchiqThreadEvent[THREAD_SYNC],
            0,
            FALSE);
    }

    if (slotZeroPtr->Slave.SyncRelease.Armed &&
        slotZeroPtr->Slave.SyncRelease.Fired) {

        slotZeroPtr->Slave.SyncRelease.Armed = 0;
        KeSetEvent(
            &deviceContextPtr->VchiqThreadEvent[THREAD_SYNC_RELEASE],
            0,
            FALSE);
    }

    return;
}

VCHIQ_NONPAGED_SEGMENT_END