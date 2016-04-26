//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//      driver.c
//
// Abstract:
//
//      This file contains the driver entry points and callbacks.
//

#include "precomp.h"

#include "trace.h"
#include "driver.tmh"

#include "slotscommon.h"
#include "device.h"
#include "driver.h"

VCHIQ_INIT_SEGMENT_BEGIN

/*++
Routine Description:

     VCHIQ DriverEntry implementation

Parameters Description:

     DriverObject - represents the instance of the function driver that is 
        loaded into memory. DriverEntry must initialize members of DriverObject
        before it returns to the caller. DriverObject is allocated by the 
        system before the driver is loaded, and it is released by the system 
        after the system unloads the function driver from memory.

     RegistryPath - represents the driver specific path in the Registry.
        The function driver can use the path to store driver related data 
        between reboots. The path does not store hardware instance specific 
        data.

Return Value:

     STATUS_SUCCESS if successful,
     STATUS_UNSUCCESSFUL otherwise.

--*/
_Use_decl_annotations_
NTSTATUS DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attributes;

    // Initialize WPP Tracing
    {
        WPP_INIT_TRACING(DriverObject, RegistryPath);
        RECORDER_CONFIGURE_PARAMS recorderConfigureParams;
        RECORDER_CONFIGURE_PARAMS_INIT(&recorderConfigureParams);
        WppRecorderConfigure(&recorderConfigureParams);
    }

    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    WDF_DRIVER_CONFIG_INIT(&config, VchiqOnDeviceAdd);
    config.EvtDriverUnload = VchiqOnDriverUnload;
    config.DriverPoolTag = VCHIQ_ALLOC_TAG_WDF;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        VCHIQ_LOG_ERROR(
            "WdfDriverCreate failed %!STATUS!",
            status);
        WPP_CLEANUP(DriverObject);
    }

    return status;
}

VCHIQ_INIT_SEGMENT_END

VCHIQ_PAGED_SEGMENT_BEGIN

/*++

Routine Description:

     Called when driver is unloaded. Clean up WPP.

Arguments:

     Driver - Handle to a framework driver object created in DriverEntry

Return Value:

     NTSTATUS

--*/
_Use_decl_annotations_
VOID VchiqOnDriverUnload (
    WDFDRIVER Driver
    )
{
    PDRIVER_OBJECT driverObjectPtr;

    PAGED_CODE();

    VCHIQ_LOG_INFORMATION("Driver unloaded");

    driverObjectPtr = WdfDriverWdmGetDriverObject(Driver);

    WPP_CLEANUP(driverObjectPtr);

    return;
}

/*++

Routine Description:

     VchiqOnDeviceAdd is called by the framework in response to AddDevice
     call from the PnP manager. We create and initialize a device object to
     represent a new instance of the device.

Arguments:

     Driver - Handle to a framework driver object created in DriverEntry

     DeviceInitPtr - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

     NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS VchiqOnDeviceAdd (
    WDFDRIVER         Driver,
    PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    status = VchiqCreateDevice(Driver, DeviceInit);

    VCHIQ_LOG_INFORMATION(
        "VchiqOnDeviceAdd Status %!STATUS!",
        status);

    return status;
}

VCHIQ_PAGED_SEGMENT_END

