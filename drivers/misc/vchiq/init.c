//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//      init.c
//
// Abstract:
//
//      This file contain early initialization related definition
//

#include "precomp.h"

#include "trace.h"
#include "init.tmh"

#include "slotscommon.h"
#include "device.h"
#include "interrupt.h"
#include "file.h"
#include "slots.h"
#include "memory.h"
#include "init.h"

VCHIQ_PAGED_SEGMENT_BEGIN

/*++

Routine Description:

     VchiqInitOperation is responsible to run initialization
        and register for mailbox driver interface.

Arguments:

     Device - A handle to a framework device object.

Return Value:

     NTSTATUS value

--*/
_Use_decl_annotations_
NTSTATUS
VchiqInitOperation (
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
    )
{
    NTSTATUS status;

    PAGED_CODE();

    // Only perform initialize if this is the first boot
    if (PreviousState != WdfPowerDeviceD3Final) {
        VCHIQ_LOG_INFORMATION(
            "This is not first boot %d do nothing",
            PreviousState);
        status = STATUS_SUCCESS;
        goto End;
    }

    DEVICE_CONTEXT* deviceContextPtr = VchiqGetDeviceContext(Device);

    // Enable interrupts
    status = VchiqEnableInterrupts(deviceContextPtr);
    if (!NT_SUCCESS(status)) {
        VCHIQ_LOG_ERROR(
            "Failed to intialize interrupt status = %!STATUS!",
            status);
        goto End;
    }

    // Very important to enable VCHIQ first. This would ensure that
    // VCHIQ interrupt are triggred first before mailbox. If mailbox
    // interrupt are triggered first VCHIQ interrupt would subsequently
    // fail to be triggered. So register for notification and send VCHIQ
    // property initlization once RPIQ driver is loaded.
    status = VchiqInit(deviceContextPtr);
    if (!NT_SUCCESS(status)) {
        VCHIQ_LOG_ERROR(
            "Failed to set initialize MAC Address %!STATUS!",
            status);
        goto End;
    }

    // Register notification for rpiq device interface
    status = IoRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        (VOID*)&RPIQ_INTERFACE_GUID,
        WdfDriverWdmGetDriverObject(WdfGetDriver()),
        VchiqInterfaceCallback,
        Device,
        &deviceContextPtr->RpiqNotificationHandle);
    if (!NT_SUCCESS(status)) {
        VCHIQ_LOG_ERROR(
            "Registrating rpiq interface notification fails %!STATUS!",
            status);
        goto End;
    }

End:
    return status;
}

/*++

Routine Description:

     MailboxInterfaceCallback is DRIVER_NOTIFICATION_CALLBACK_ROUTINE callback
         to ensure Slot memory is registered thru mailbox as soon the mailbox 
         driver comes online.

Arguments:

     NotificationStructure - The DEVICE_INTERFACE_CHANGE_NOTIFICATION structure
          describes a device interface that has been enabled (arrived) or
          disabled (removed).

     Context - The callback routine's Context parameter contains the context
          data the driver supplied during registration.

Return Value:

     NTSTATUS value

--*/
_Use_decl_annotations_
NTSTATUS VchiqInterfaceCallback (
    VOID* NotificationStructure,
    VOID* Context
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE device = Context;
    DEVICE_INTERFACE_CHANGE_NOTIFICATION* notification;
    ULONG RpiqIoctlBuffer[2] = { 0 };
    WDF_MEMORY_DESCRIPTOR InputDescriptor;

    PAGED_CODE();

    notification =
        (DEVICE_INTERFACE_CHANGE_NOTIFICATION*)NotificationStructure;

    if (IsEqualGUID(&notification->Event, &GUID_DEVICE_INTERFACE_ARRIVAL)) {
        WDF_OBJECT_ATTRIBUTES  ioTargetAttrib;
        WDFIOTARGET  ioTarget;
        WDF_IO_TARGET_OPEN_PARAMS  openParams;

#pragma prefast(suppress:28922, "Check device value anyway to satisfy WdfIoTargetCreate requirement")
        if (device == NULL) {
            status = STATUS_INVALID_PARAMETER;
            VCHIQ_LOG_ERROR("Fail to create remote target %!STATUS!", status);
            return status;
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&ioTargetAttrib);
        ioTargetAttrib.ParentObject = device;

        status = WdfIoTargetCreate(
            device,
            &ioTargetAttrib,
            &ioTarget);
        if (!NT_SUCCESS(status)) {
            VCHIQ_LOG_ERROR("Fail to create remote target %!STATUS!", status);
            goto End;
        }
        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
            &openParams,
            notification->SymbolicLinkName,
            STANDARD_RIGHTS_ALL);

        status = WdfIoTargetOpen(
            ioTarget,
            &openParams);
        if (!NT_SUCCESS(status)) {
            WdfObjectDelete(ioTarget);
            VCHIQ_LOG_ERROR(
                "Fail to create rpiq remote target %!STATUS!",
                status);
            goto End;
        }

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputDescriptor,
            (VOID*)RpiqIoctlBuffer,
            sizeof(RpiqIoctlBuffer));

        RpiqIoctlBuffer[0] = MAILBOX_CHANNEL_VCHIQ;
        RpiqIoctlBuffer[1] = VchiqGetDeviceContext(device)->SlotMemoryPhy.u.LowPart
            | OFFSET_DIRECT_SDRAM;

        status = WdfIoTargetSendIoctlSynchronously(
            ioTarget,
            NULL,
            IOCTL_MAILBOX_VCHIQ,
            &InputDescriptor,
            NULL,
            NULL,
            NULL);
        if (!NT_SUCCESS(status)) {
            WdfObjectDelete(ioTarget);
            VCHIQ_LOG_ERROR(
                "WdfIoTargetSendIoctlSynchronously failed %!STATUS!",
                status);
            goto End;
        }

        WdfObjectDelete(ioTarget);
    }

End:
    return status;
}

VCHIQ_PAGED_SEGMENT_END