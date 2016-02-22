/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:

    This file contains the driver entry points and callbacks.

--*/

#include "driver.h"
#include "driver.tmh"


#pragma code_seg("INIT")
_Use_decl_annotations_
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath
)
/*++

Routine Description:

    Create the driver object

Arguments:

    DriverObject - Pointer to the driver object
    RegistryPath - Driver specific registry path

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attributes;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING( DriverObject, RegistryPath );

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    WDF_DRIVER_CONFIG_INIT(&config, OnDeviceAdd);
    config.EvtDriverUnload = OnDriverUnload;

    status = WdfDriverCreate(
                DriverObject,
                RegistryPath,
                &attributes,
                &config,
                WDF_NO_HANDLE);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_INIT, "BCM2836PWM WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
    }

    return status;
}

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
OnDriverUnload (  
    WDFDRIVER Driver
)
/*++

Routine Description:

    Called when driver is unloaded. Clean up WPP.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

Return Value:

    None

--*/
    {
    PAGED_CODE();

    PDRIVER_OBJECT driverObject;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_INIT, "Driver unloaded");

    driverObject = WdfDriverWdmGetDriverObject(Driver);
    WPP_CLEANUP(driverObject);
}

