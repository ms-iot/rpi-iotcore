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
#include "precomp.h"

#include "i2ctrace.h"
#include "bcmi2c.h"
#include "device.h"
#include "driver.h"

#include "driver.tmh"

BCM_I2C_PAGED_SEGMENT_BEGIN; //================================================

_Use_decl_annotations_
NTSTATUS OnDeviceAdd (WDFDRIVER /*WdfDriver*/, WDFDEVICE_INIT* DeviceInitPtr)
{
    PAGED_CODE();
    BCM_I2C_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    NTSTATUS status;

    //
    // Configure DeviceInit structure
    //
    status = SpbDeviceInitConfig(DeviceInitPtr);
    if (!NT_SUCCESS(status)) {
        BSC_LOG_ERROR(
            "SpbDeviceInitConfig() failed. (DeviceInitPtr = %p, status = %!STATUS!)",
            DeviceInitPtr,
            status);
        return status;
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

        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInitPtr, &pnpCallbacks);
    }

    //
    // Create the device.
    //
    WDFDEVICE wdfDevice;
    BCM_I2C_DEVICE_CONTEXT* devicePtr;
    {
        WDF_OBJECT_ATTRIBUTES deviceAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
            &deviceAttributes,
            BCM_I2C_DEVICE_CONTEXT);

        status = WdfDeviceCreate(
            &DeviceInitPtr,
            &deviceAttributes,
            &wdfDevice);
        if (!NT_SUCCESS(status)) {
            BSC_LOG_ERROR(
                "Failed to create WDFDEVICE. (DeviceInitPtr = %p, status = %!STATUS!)",
                DeviceInitPtr,
                status);
            return status;
        }

        devicePtr = GetDeviceContext(wdfDevice);
        NT_ASSERT(devicePtr);
        devicePtr->WdfDevice = wdfDevice;
    }

    //
    // Ensure device is disable-able
    //
    {
        WDF_DEVICE_STATE deviceState;
        WDF_DEVICE_STATE_INIT(&deviceState);
        deviceState.NotDisableable = WdfFalse;
        WdfDeviceSetDeviceState(wdfDevice, &deviceState);
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

        spbConfig.EvtSpbTargetConnect = OnTargetConnect;

        //
        // Register for IO callbacks.
        //

        spbConfig.ControllerDispatchType = WdfIoQueueDispatchSequential;
        spbConfig.EvtSpbIoRead = OnRead;
        spbConfig.EvtSpbIoWrite = OnWrite;
        spbConfig.EvtSpbIoSequence = OnSequence;

        status = SpbDeviceInitialize(wdfDevice, &spbConfig);
        if (!NT_SUCCESS(status)) {
            BSC_LOG_ERROR(
                "SpbDeviceInitialize failed. (wdfDevice = %p, status = %!STATUS!)",
                wdfDevice,
                status);
            return status;
        }
    }

    //
    // Set target object attributes.
    //
    {
        WDF_OBJECT_ATTRIBUTES targetAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
            &targetAttributes,
            BCM_I2C_TARGET_CONTEXT);

        SpbControllerSetTargetAttributes(wdfDevice, &targetAttributes);
    }

    //
    // Create an interrupt object
    //
    BCM_I2C_INTERRUPT_CONTEXT* interruptContextPtr;
    {
        WDF_OBJECT_ATTRIBUTES interruptObjectAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
            &interruptObjectAttributes,
            BCM_I2C_INTERRUPT_CONTEXT);

        WDF_INTERRUPT_CONFIG interruptConfig;
        WDF_INTERRUPT_CONFIG_INIT(
            &interruptConfig,
            OnInterruptIsr,
            OnInterruptDpc);

        status = WdfInterruptCreate(
            wdfDevice,
            &interruptConfig,
            &interruptObjectAttributes,
            &devicePtr->WdfInterrupt);
        if (!NT_SUCCESS(status)) {
            BSC_LOG_ERROR(
                "Failed to create interrupt object. (wdfDevice = %p, status = %!STATUS!)",
                wdfDevice,
                status);
            return status;
        }

        interruptContextPtr = GetInterruptContext(devicePtr->WdfInterrupt);
        interruptContextPtr->WdfInterrupt = devicePtr->WdfInterrupt;
    }
    devicePtr->InterruptContextPtr = interruptContextPtr;

    NT_ASSERT(status == STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID OnDriverUnload ( WDFDRIVER WdfDriver )
{
    PAGED_CODE();

    DRIVER_OBJECT* driverObjectPtr = WdfDriverWdmGetDriverObject(WdfDriver);
    WPP_CLEANUP(driverObjectPtr);
}

BCM_I2C_PAGED_SEGMENT_END; //==================================================
BCM_I2C_INIT_SEGMENT_BEGIN; //=================================================

_Use_decl_annotations_
NTSTATUS DriverEntry (
    DRIVER_OBJECT* DriverObjectPtr,
    UNICODE_STRING* RegistryPathPtr
    )
{
    PAGED_CODE();

    //
    // Initialize logging
    //
    {
        WPP_INIT_TRACING(DriverObjectPtr, RegistryPathPtr);
        RECORDER_CONFIGURE_PARAMS recorderConfigureParams;
        RECORDER_CONFIGURE_PARAMS_INIT(&recorderConfigureParams);
        WppRecorderConfigure(&recorderConfigureParams);
#if DBG
        WPP_RECORDER_LEVEL_FILTER(BSC_TRACING_VERBOSE) = TRUE;
#endif // DBG
    }

    NTSTATUS status;

    WDFDRIVER wdfDriver;
    {
        WDF_DRIVER_CONFIG wdfDriverConfig;
        WDF_DRIVER_CONFIG_INIT(&wdfDriverConfig, OnDeviceAdd);
        wdfDriverConfig.DriverPoolTag = BCM_I2C_POOL_TAG;
        wdfDriverConfig.EvtDriverUnload = OnDriverUnload;

        status = WdfDriverCreate(
                DriverObjectPtr,
                RegistryPathPtr,
                WDF_NO_OBJECT_ATTRIBUTES,
                &wdfDriverConfig,
                &wdfDriver);
        if (!NT_SUCCESS(status)) {
            BSC_LOG_ERROR(
                "Failed to create WDF driver object. (DriverObjectPtr = %p, RegistryPathPtr = %p)",
                DriverObjectPtr,
                RegistryPathPtr);
            return status;
        }
    }

    NT_ASSERT(status == STATUS_SUCCESS);
    return status;
}

BCM_I2C_INIT_SEGMENT_END; //===================================================
