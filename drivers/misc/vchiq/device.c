//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//      device.c
//
// Abstract:
//
//      VCHIQ WDF device implementation
//

#include "precomp.h"

#include "trace.h"
#include "device.tmh"

#include "slotscommon.h"
#include "device.h"
#include "interrupt.h"
#include "init.h"
#include "file.h"
#include "ioctl.h"
#include "slots.h"

VCHIQ_PAGED_SEGMENT_BEGIN

/*++

Routine Description:

     Worker routine called to create a device and its software resources.

Arguments:

     DeviceInitPtr - Pointer to an opaque init structure. Memory for this
          structure will be freed by the framework when the WdfDeviceCreate
          succeeds. So don't access the structure after that point.

Return Value:

     NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS VchiqCreateDevice (
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInitPtr
    )
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    DEVICE_CONTEXT* deviceContextPtr;
    WDFDEVICE device;
    NTSTATUS status;
    WDF_IO_TYPE_CONFIG ioConfig;
    DECLARE_CONST_UNICODE_STRING(vchiqSymbolicLink, VCHIQ_SYMBOLIC_NAME);

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    {
        WDF_FILEOBJECT_CONFIG fileobjectConfig;
        WDF_FILEOBJECT_CONFIG_INIT(
            &fileobjectConfig,
            WDF_NO_EVENT_CALLBACK,
            VchiqFileClose,
            WDF_NO_EVENT_CALLBACK);

        fileobjectConfig.FileObjectClass = WdfFileObjectWdfCanUseFsContext;

        WdfDeviceInitSetFileObjectConfig(
            DeviceInitPtr,
            &fileobjectConfig,
            WDF_NO_OBJECT_ATTRIBUTES);
    }

    WDF_IO_TYPE_CONFIG_INIT(&ioConfig);
    ioConfig.ReadWriteIoType = WdfDeviceIoDirect;
    ioConfig.DeviceControlIoType = WdfDeviceIoDirect;
    ioConfig.DirectTransferThreshold = 0;

    WdfDeviceInitSetIoTypeEx(DeviceInitPtr, &ioConfig);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = 
        VchiqPrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware =
        VchiqReleaseHardware;
    pnpPowerCallbacks.EvtDeviceD0EntryPostInterruptsEnabled = 
        VchiqInitOperation;
    WdfDeviceInitSetPnpPowerEventCallbacks(
        DeviceInitPtr, 
        &pnpPowerCallbacks);

    WdfDeviceInitSetIoInCallerContextCallback(
        DeviceInitPtr,
        VchiqInCallerContext);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &deviceAttributes, 
        DEVICE_CONTEXT);

    status = WdfDeviceCreate(
        &DeviceInitPtr,
        &deviceAttributes,
        &device);
    if (!NT_SUCCESS(status)) {
        VCHIQ_LOG_ERROR(
            "WdfDeviceCreate fail %!STATUS!", status);
        goto End;
    }

    {
        WDF_OBJECT_ATTRIBUTES attributes;
        WDF_IO_QUEUE_CONFIG queueConfig;
        WDFQUEUE queue;

        deviceContextPtr = VchiqGetDeviceContext(device);
        deviceContextPtr->Device = device;
        deviceContextPtr->VersionMajor = VCHIQ_VERSION_MAJOR;
        deviceContextPtr->VersionMinor = VCHIQ_VERSION_MINOR;

        deviceContextPtr->PhyDeviceObjectPtr =
            WdfDeviceWdmGetPhysicalDevice(device);

        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
            &queueConfig,
            WdfIoQueueDispatchParallel);
        queueConfig.EvtIoDeviceControl = VchiqIoDeviceControl;
        queueConfig.EvtIoStop = VchiqIoStop;

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ExecutionLevel = WdfExecutionLevelPassive;

        status = WdfIoQueueCreate(
            device,
            &queueConfig,
            &attributes,
            &queue);
        if (!NT_SUCCESS(status)) {
            VCHIQ_LOG_ERROR(
                "WdfIoQueueCreate fail %!STATUS!", status);
            goto End;
        }
    }

    // Create symbolic and device interface
    status = WdfDeviceCreateSymbolicLink(
        device,
        &vchiqSymbolicLink);
    if (!NT_SUCCESS(status)) {
        VCHIQ_LOG_ERROR(
            "Fail to register symbolic link %!STATUS!",
            status);
        goto End;
    }

End:
    VCHIQ_LOG_INFORMATION("Exit Status %!STATUS!", status);

    return status;
}

/*++

Routine Description:

     In this callback, the driver does whatever is necessary to make the
     hardware ready to use.

Arguments:

     Device - A handle to a framework device object.

     ResourcesRaw - A handle to a framework resource-list object that identifies
          the raw hardware resources that the Plug and Play manager has assigned
          to the device.

     ResourcesTranslated - A handle to a framework resource-list object that
          identifies the translated hardware resources that the Plug and Play
          manager has assigned to the device.

Return Value:

     NTSTATUS value

--*/
_Use_decl_annotations_
NTSTATUS VchiqPrepareHardware (
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesRaw,
    WDFCMRESLIST ResourcesTranslated
    )
{
    NTSTATUS status;
    DEVICE_CONTEXT* deviceContextPtr;
    ULONG resourceCount;
    ULONG MemoryResourceCount = 0;
    ULONG InterruptResourceCount = 0;

    PAGED_CODE();

    deviceContextPtr = VchiqGetDeviceContext(Device);
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    for (ULONG i = 0; i < resourceCount; ++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR res;

        res = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        switch (res->Type)
        {
        case CmResourceTypeMemory:
            {
                VCHIQ_LOG_INFORMATION(
                    "Memory Resource Start: 0x%08x, Length: 0x%08x",
                    res->u.Memory.Start.LowPart,
                    res->u.Memory.Length);

                deviceContextPtr->VchiqRegisterPtr =
                    MmMapIoSpaceEx(
                        res->u.Memory.Start,
                        res->u.Memory.Length,
                        PAGE_READWRITE | PAGE_NOCACHE);
                if (deviceContextPtr->VchiqRegisterPtr == NULL) {
                    VCHIQ_LOG_ERROR("Failed to map VCHIQ register");
                    status = STATUS_UNSUCCESSFUL;
                    goto End;
                }
                deviceContextPtr->VchiqRegisterLength = res->u.Memory.Length;
                ++MemoryResourceCount;
            }
            break;
        case CmResourceTypeInterrupt:
            {
                VCHIQ_LOG_INFORMATION(
                    "Interrupt Level: 0x%08x, Vector: 0x%08x\n",
                    res->u.Interrupt.Level,
                    res->u.Interrupt.Vector);

                WDF_INTERRUPT_CONFIG interruptConfig;

                WDF_INTERRUPT_CONFIG_INIT (
                    &interruptConfig,
                    VchiqIsr,
                    VchiqDpc);

                interruptConfig.InterruptRaw =
                    WdfCmResourceListGetDescriptor (ResourcesRaw, i);
                interruptConfig.InterruptTranslated = res;

                status = WdfInterruptCreate(
                    Device,
                    &interruptConfig,
                    WDF_NO_OBJECT_ATTRIBUTES,
                    &deviceContextPtr->VchiqIntObj);
                if (!NT_SUCCESS (status)) {
                    VCHIQ_LOG_ERROR (
                        "Fail to initialize VCHIQ interrupt object");
                    return status;
                }
                ++InterruptResourceCount;
            }
            break;
        default:
            {
                VCHIQ_LOG_WARNING("Unsupported resources, ignoring");
            }
            break;
        }
        if (MemoryResourceCount && InterruptResourceCount)
            break;
    }

    if (MemoryResourceCount != VCHIQ_MEMORY_RESOURCE_TOTAL &&
        InterruptResourceCount != VCHIQ_INT_RESOURCE_TOTAL) {
        status = STATUS_UNSUCCESSFUL;
        VCHIQ_LOG_ERROR("Unknown resource assignment");
        goto End;
    }

    status = STATUS_SUCCESS;

End:
    if (!NT_SUCCESS(status)) {
        VCHIQ_LOG_ERROR("VchiqPrepareHardware failed %!STATUS!", status);
    }

    return status;
}

/*++

Routine Description:

     EvtDeviceReleaseHardware for Vchiq.

Arguments:

     Device - A handle to a framework device object.

     ResourcesTranslated - A handle to a resource list object that identifies
          the translated hardware resources that the Plug and Play manager has
          assigned to the device.

Return Value:

     NTSTATUS value

--*/
_Use_decl_annotations_
NTSTATUS VchiqReleaseHardware (
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesTranslated
    )
{
    NTSTATUS status;
    DEVICE_CONTEXT* deviceContextPtr;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    deviceContextPtr = VchiqGetDeviceContext(Device);


    if (deviceContextPtr->VchiqRegisterPtr != NULL) {
        MmUnmapIoSpace(deviceContextPtr->VchiqRegisterPtr,
            deviceContextPtr->VchiqRegisterLength);
        deviceContextPtr->VchiqRegisterPtr = NULL;
    }

    status = VchiqRelease(deviceContextPtr);
    if (!NT_SUCCESS(status)) {
        VCHIQ_LOG_ERROR("Fail to release VCHIQ resource %!STATUS!", status);
    }

    if (deviceContextPtr->RpiqNotificationHandle != NULL) {

        status = IoUnregisterPlugPlayNotification(
            deviceContextPtr->RpiqNotificationHandle);
        if (!NT_SUCCESS(status)) {
            VCHIQ_LOG_ERROR(
                "Rpiq interface notification deregistration fails %!STATUS!",
                status);
        }

        deviceContextPtr->RpiqNotificationHandle = NULL;
    }

    NT_ASSERT(deviceContextPtr->AllocPhyMemCount == 0);

    return STATUS_SUCCESS;
}

VCHIQ_PAGED_SEGMENT_END

VCHIQ_NONPAGED_SEGMENT_BEGIN

/*++

Routine Description:

     A driver's EvtIoStop event callback function completes, requeues, or
     suspends processing of a specified request because the request's I/O queue
     is being stopped.

Arguments:

     Queue - A handle to the framework queue object that is associated with the
          I/O request.

     WdfRequest - A handle to a framework request object.

     ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS
          typed flags that identify the reason that the callback function is
          being called and whether the request is cancelable.

Return Value:

     NTSTATUS value

--*/
_Use_decl_annotations_
VOID VchiqIoStop (
    WDFQUEUE Queue,
    WDFREQUEST WdfRequest,
    ULONG ActionFlags
    )
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(ActionFlags);

    // Requeue all pending request
    WdfRequestStopAcknowledge(
        WdfRequest,
        TRUE);

    return;
}

VCHIQ_NONPAGED_SEGMENT_END



