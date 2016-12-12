// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    initunlo.c
//
// Abstract:
//
//    This module contains the code that is very specific to initialization
//    and unload operations in the serial driver

#include "precomp.h"
#include "initunlo.tmh"

static const PHYSICAL_ADDRESS SerialPhysicalZero = {0};

//
// We use this to query into the registry as to whether we
// should break at driver entry.
//

SERIAL_FIRMWARE_DATA DriverDefaults;

//
// This is exported from the kernel.  It is used to point
// to the address that the kernel debugger is using.
//
extern PUCHAR* KdComPortInUse;
//
// INIT - only needed during init and then can be disposed
// PAGESRP0 - always paged / never locked
// PAGESER - must be locked when a device is open, else paged
//
//
// INIT is used for DriverEntry() specific code
//
// PAGESRP0 is used for code that is not often called and has nothing
// to do with I/O performance.  An example, passive-level PNP
// support functions
//
// PAGESER is used for code that needs to be locked after an open for both
// performance and IRQL reasons.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, SerialEvtDriverContextCleanup)
#endif

/*++
Routine Description:

     mini Uart DriverEntry implementation

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
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;
    WDFDRIVER hDriver;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    // Initialize WPP Tracing

    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT,
                    "RPi3 miniUart driver based on Serial Sample (WDF Version) "
                    "\r\n++DriverEntry()\r\n");

    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = SerialEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config, SerialEvtDeviceAdd);

    status = WdfDriverCreate(DriverObject,
                           RegistryPath,
                           &attributes,
                           &config,
                           &hDriver);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT,
                         "WdfDriverCreate failed with status 0x%x\n",
                         status);

        // Cleanup tracing here because DriverContextCleanup will not be called
        // as we have failed to create WDFDRIVER object itself.
        // Please note that if your return failure from DriverEntry after the
        // WDFDRIVER object is created successfully, you don't have to
        // call WPP cleanup because in those cases DriverContextCleanup
        // will be executed when the framework deletes the DriverObject.

        WPP_CLEANUP(DriverObject);
        return status;
    }

    //
    // Call to find out default values to use for all the devices that the
    // driver controls, including whether or not to break on entry.
    //

    SerialGetConfigDefaults(&DriverDefaults, hDriver);

    // Break on entry if requested via registry

    if (DriverDefaults.ShouldBreakOnEntry) {
        DbgBreakPoint();
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "--DriverEntry()=%Xh\r\n",status);
    return status;
}


/*++
Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    Driver - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
_Use_decl_annotations_
VOID
SerialEvtDriverContextCleanup(
    WDFOBJECT Driver
    )
{
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE ();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "++SerialEvtDriverContextCleanup\n");

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "--SerialEvtDriverContextCleanup\n");

    // Stop WPP Tracing
    WPP_CLEANUP( WdfDriverWdmGetDriverObject(Driver) );
}



