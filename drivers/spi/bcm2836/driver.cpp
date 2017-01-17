/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name: 

    driver.cpp

Abstract:

    This module contains the WDF driver initialization 
    functions for the controller driver.

Environment:

    kernel-mode only

Revision History:

--*/

#include "internal.h"
#include "driver.h"
#include "device.h"
#include "ntstrsafe.h"

#include "driver.tmh"

_Use_decl_annotations_
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG driverConfig;
    WDF_OBJECT_ATTRIBUTES driverAttributes;

    WDFDRIVER fxDriver;

    NTSTATUS status;

    WPP_INIT_TRACING(DriverObject, RegistryPath);

    FuncEntry(TRACE_FLAG_WDFLOADING);

    WDF_DRIVER_CONFIG_INIT(&driverConfig, OnDeviceAdd);
    driverConfig.DriverPoolTag = BCMS_POOL_TAG;

    WDF_OBJECT_ATTRIBUTES_INIT(&driverAttributes);
    driverAttributes.EvtCleanupCallback = OnDriverCleanup;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &driverAttributes,
        &driverConfig,
        &fxDriver);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR, 
            TRACE_FLAG_WDFLOADING,
            "Error creating WDF driver object - %!STATUS!", 
            status);

        goto exit;
    }

    Trace(
        TRACE_LEVEL_VERBOSE, 
        TRACE_FLAG_WDFLOADING,
        "Created WDFDRIVER %p",
        fxDriver);

exit:

    FuncExit(TRACE_FLAG_WDFLOADING);

    return status;
}

_Use_decl_annotations_
VOID
OnDriverCleanup(
    WDFOBJECT Object
    )
{
    UNREFERENCED_PARAMETER(Object);

    WPP_CLEANUP(NULL);
}

_Use_decl_annotations_
NTSTATUS
OnDeviceAdd(
    WDFDRIVER FxDriver,
    PWDFDEVICE_INIT FxDeviceInit
    )
/*++
 
  Routine Description:

    This routine creates the device object for an SPB 
    controller and the device's child objects.

  Arguments:

    FxDriver - the WDF driver object handle
    FxDeviceInit - information about the PDO that we are loading on

  Return Value:

    Status

--*/
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    PPBC_DEVICE pDevice;
    NTSTATUS status;
    
    UNREFERENCED_PARAMETER(FxDriver);

    //
    // Configure DeviceInit structure
    //

    status = SpbDeviceInitConfig(FxDeviceInit);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR, 
            TRACE_FLAG_WDFLOADING,
            "Failed SpbDeviceInitConfig() for WDFDEVICE_INIT %p - %!STATUS!", 
            FxDeviceInit,
            status);

        goto exit;
    }  
        
    //
    // Setup PNP/Power callbacks.
    //

    {
        WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

        pnpCallbacks.EvtDevicePrepareHardware = OnPrepareHardware;
        pnpCallbacks.EvtDeviceReleaseHardware = OnReleaseHardware;
        pnpCallbacks.EvtDeviceD0Entry = OnD0Entry;
        pnpCallbacks.EvtDeviceD0Exit = OnD0Exit;
        pnpCallbacks.EvtDeviceSelfManagedIoInit = OnSelfManagedIoInit;
        pnpCallbacks.EvtDeviceSelfManagedIoCleanup = OnSelfManagedIoCleanup;

        WdfDeviceInitSetPnpPowerEventCallbacks(FxDeviceInit, &pnpCallbacks);
    }

    //
    // Create the device.
    //

    {
        WDF_OBJECT_ATTRIBUTES deviceAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, PBC_DEVICE);
        deviceAttributes.EvtCleanupCallback = OnDeviceCleanup;

        WDFDEVICE fxDevice;
        status = WdfDeviceCreate(
            &FxDeviceInit, 
            &deviceAttributes,
            &fxDevice);

        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "Failed to create WDFDEVICE from WDFDEVICE_INIT %p - %!STATUS!", 
                FxDeviceInit,
                status);

            goto exit;
        }

        pDevice = GetDeviceContext(fxDevice);
        NT_ASSERT(pDevice != NULL);

        pDevice->FxDevice = fxDevice;
    }
        
    //
    // Ensure device is disable-able
    //
    
    {
        WDF_DEVICE_STATE deviceState;
        WDF_DEVICE_STATE_INIT(&deviceState);
        
        deviceState.NotDisableable = WdfFalse;
        WdfDeviceSetDeviceState(pDevice->FxDevice, &deviceState);
    }
    
    //
    // Bind a SPB controller object to the device.
    //

    {
        SPB_CONTROLLER_CONFIG spbConfig;
        SPB_CONTROLLER_CONFIG_INIT(&spbConfig);

        //
        // Register for target connect callback.  The driver
        // does not need to respond to target disconnect.
        //

        spbConfig.EvtSpbTargetConnect    = OnTargetConnect;

        //
        // Register for IO callbacks.
        //

        spbConfig.ControllerDispatchType = WdfIoQueueDispatchSequential;
        spbConfig.PowerManaged           = WdfTrue;
        spbConfig.EvtSpbIoRead           = OnRead;
        spbConfig.EvtSpbIoWrite          = OnWrite;
        spbConfig.EvtSpbIoSequence       = OnSequenceRequest;
        spbConfig.EvtSpbControllerLock   = OnControllerLock;
        spbConfig.EvtSpbControllerUnlock = OnControllerUnlock;

        status = SpbDeviceInitialize(pDevice->FxDevice, &spbConfig);
       
        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "Failed SpbDeviceInitialize() for WDFDEVICE %p - %!STATUS!", 
                pDevice->FxDevice,
                status);

            goto exit;
        }

        //
        // Register for IO other callbacks.
        //
        
        SpbControllerSetIoOtherCallback(
            pDevice->FxDevice,
            OnOther,
            OnOtherInCallerContext);
    }

    //
    // Set target object attributes.
    //

    {
        WDF_OBJECT_ATTRIBUTES targetAttributes; 
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&targetAttributes, PBC_TARGET);

        SpbControllerSetTargetAttributes(pDevice->FxDevice, &targetAttributes);
    }

    //
    // Set request object attributes.
    //

    {
        WDF_OBJECT_ATTRIBUTES requestAttributes; 
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&requestAttributes, PBC_REQUEST);
        
        //
        // NOTE: Be mindful when registering for EvtCleanupCallback or 
        //       EvtDestroyCallback. IO requests arriving in the class
        //       extension, but not presented to the driver (due to
        //       cancellation), will still have their cleanup and destroy 
        //       callbacks invoked.
        //

        SpbControllerSetRequestAttributes(pDevice->FxDevice, &requestAttributes);
    }

    //
    // Create the spin lock to synchronize access
    // to the controller driver.
    //
    
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = pDevice->FxDevice;

    status = WdfSpinLockCreate(
       &attributes,
       &pDevice->Lock);
    
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR, 
            TRACE_FLAG_WDFLOADING, 
            "Failed to create device spinlock for WDFDEVICE %p - %!STATUS!", 
            pDevice->FxDevice,
            status);

        goto exit;
    }
    
    //
    // Configure idle settings to use system
    // managed idle timeout.
    //
    {    
        WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
        WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(
            &idleSettings, 
            IdleCannotWakeFromS0);

        //
        // Explicitly set initial idle timeout delay.
        //

        idleSettings.IdleTimeoutType = SystemManagedIdleTimeoutWithHint;
        idleSettings.IdleTimeout = IDLE_TIMEOUT_MONITOR_ON;

        status = WdfDeviceAssignS0IdleSettings(
            pDevice->FxDevice, 
            &idleSettings);

        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_WDFLOADING,
                "Failed to initalize S0 idle settings for WDFDEVICE %p- %!STATUS!",
                pDevice->FxDevice,
                status);
                
            goto exit;
        }
    }

    {
        KeInitializeEvent(
            &pDevice->TransferThreadWakeEvt,
            SynchronizationEvent,
            FALSE);

        pDevice->TransferThreadShutdown = 0;

        OBJECT_ATTRIBUTES objectAttributes;
        InitializeObjectAttributes(&objectAttributes,
                                   NULL,
                                   OBJ_KERNEL_HANDLE,
                                   NULL,
                                   NULL);
        HANDLE transferThread;

        status = PsCreateSystemThread(
            &transferThread,
            THREAD_ALL_ACCESS,
            NULL,
            NULL,
            NULL,
            TransferPollModeThread,
            (PVOID)pDevice->FxDevice);
        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_WDFLOADING,
                "Failed to create transfer thread WDFDEVICE %p- %!STATUS!",
                pDevice->FxDevice,
                status);

            goto exit;
        }

        status = ObReferenceObjectByHandle(transferThread,
                                      THREAD_ALL_ACCESS,
                                      NULL,
                                      KernelMode,
                                      &pDevice->pTransferThread,
                                      NULL);
        NT_ASSERT(NT_SUCCESS(status));

        ZwClose(transferThread);
    }

exit:

    FuncExit(TRACE_FLAG_WDFLOADING);

    return status;
}

_Use_decl_annotations_
VOID
OnDeviceCleanup(
    WDFOBJECT Object
    )
{
    PPBC_DEVICE pDevice = GetDeviceContext((WDFDEVICE)Object);

    //
    // Signal transfer thread to shutdown and wait indefinitely
    // for it to exit
    //

    InterlockedOr(&pDevice->TransferThreadShutdown, LONG(1));

    (void)KeSetEvent(&pDevice->TransferThreadWakeEvt, 0, FALSE);

    (void)KeWaitForSingleObject(
        pDevice->pTransferThread,
        Executive,
        KernelMode, 
        FALSE,
        NULL);

    ObDereferenceObject(pDevice->pTransferThread);
}