/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name: 

    device.cpp

Abstract:

    This module contains WDF device initialization 
    and SPB callback functions for the controller driver.

Environment:

    kernel-mode only

Revision History:

--*/

#include "internal.h"
#include "device.h"
#include "controller.h"

#include "device.tmh"


/////////////////////////////////////////////////
//
// WDF and SPB DDI callbacks.
//
/////////////////////////////////////////////////

_Use_decl_annotations_
NTSTATUS
OnPrepareHardware(
    WDFDEVICE FxDevice,
    WDFCMRESLIST FxResourcesRaw,
    WDFCMRESLIST FxResourcesTranslated
    )
/*++
 
  Routine Description:

    This routine maps the hardware resources to the SPB
    controller register structure.

  Arguments:

    FxDevice - a handle to the framework device object
    FxResourcesRaw - list of translated hardware resources that 
        the PnP manager has assigned to the device
    FxResourcesTranslated - list of raw hardware resources that 
        the PnP manager has assigned to the device

  Return Value:

    Status

--*/
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    PPBC_DEVICE pDevice = GetDeviceContext(FxDevice);
    NT_ASSERT(pDevice != NULL);

    ULONG irqCount = 0;
    NTSTATUS status = STATUS_SUCCESS; 

    UNREFERENCED_PARAMETER(FxResourcesRaw);
    
    //
    // Get the register base for the SPI controller.
    //

    {
        ULONG resourceCount = WdfCmResourceListGetCount(FxResourcesTranslated);

        for (ULONG i = 0; i < resourceCount; i++)
        {
            PCM_PARTIAL_RESOURCE_DESCRIPTOR res;

            res = WdfCmResourceListGetDescriptor(FxResourcesTranslated, i);

            if (res->Type == CmResourceTypeMemory)
            {
                if (pDevice->pSPIRegisters != NULL)
                {
                    status = STATUS_DEVICE_CONFIGURATION_ERROR;
                    Trace(
                        TRACE_LEVEL_ERROR,
                        TRACE_FLAG_WDFLOADING,
                        "Error multiple memory regions assigned (PA:%I64x, length:%d) for WDFDEVICE %p - %!STATUS!",
                        res->u.Memory.Start.QuadPart,
                        res->u.Memory.Length,
                        pDevice->FxDevice,
                        status);
                    goto exit;
                }

                if (res->u.Memory.Length < sizeof(BCM_SPI_REGISTERS))
                {
                    status = STATUS_DEVICE_CONFIGURATION_ERROR;

                    Trace(
                        TRACE_LEVEL_ERROR,
                        TRACE_FLAG_WDFLOADING,
                        "Error memory region too small (PA:%I64x, length:%d) for WDFDEVICE %p - %!STATUS!",
                        res->u.Memory.Start.QuadPart,
                        res->u.Memory.Length,
                        pDevice->FxDevice,
                        status);
                    goto exit;
                }

                pDevice->pSPIRegisters =
#if (NTDDI_VERSION > NTDDI_WINBLUE)
                    (PBCM_SPI_REGISTERS)MmMapIoSpaceEx(
                    res->u.Memory.Start,
                    res->u.Memory.Length,
                    PAGE_READWRITE | PAGE_NOCACHE);
#else
                    (PBCM_SPI_REGISTERS)MmMapIoSpace(
                    res->u.Memory.Start,
                    res->u.Memory.Length,
                    MmNonCached);
#endif

                if (pDevice->pSPIRegisters == NULL)
                {
                    status = STATUS_INSUFFICIENT_RESOURCES;

                    Trace(
                        TRACE_LEVEL_ERROR,
                        TRACE_FLAG_WDFLOADING,
                        "Error mapping controller registers (PA:%I64x, length:%d) for WDFDEVICE %p - %!STATUS!",
                        res->u.Memory.Start.QuadPart,
                        res->u.Memory.Length,
                        pDevice->FxDevice,
                        status);

                    goto exit;
                }

                pDevice->SPIRegistersCb = res->u.Memory.Length;
                pDevice->pSPIRegistersPhysicalAddress = res->u.Memory.Start;

                Trace(
                    TRACE_LEVEL_INFORMATION,
                    TRACE_FLAG_WDFLOADING,
                    "SPI controller @ paddr %I64x vaddr @ %p for WDFDEVICE %p",
                    pDevice->pSPIRegistersPhysicalAddress.QuadPart,
                    pDevice->pSPIRegisters,
                    pDevice->FxDevice);
            }
            else if (res->Type == CmResourceTypeInterrupt)
            {
                irqCount++;
            }
        }
    }

    if (irqCount != 1)
    {
        status = STATUS_DEVICE_CONFIGURATION_ERROR;
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_WDFLOADING,
            "Error number of assigned interrupts incorrect (%d instead of 1) for WDFDEVICE %p - %!STATUS!",
            irqCount,
            pDevice->FxDevice,
            status);
        goto exit;
    }

    if (pDevice->pSPIRegisters == NULL)
    {
        status = STATUS_DEVICE_CONFIGURATION_ERROR;
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_WDFLOADING,
            "Error memory region missing for WDFDEVICE %p - %!STATUS!",
            pDevice->FxDevice,
            status);
        goto exit;
    }

exit:

    if (!NT_SUCCESS(status))
    {
        // make sure memory is unmapped in case of failure
        OnReleaseHardware(FxDevice, FxResourcesTranslated);
    }

    FuncExit(TRACE_FLAG_WDFLOADING);

    return status;
}

_Use_decl_annotations_
NTSTATUS
OnReleaseHardware(
    WDFDEVICE FxDevice,
    WDFCMRESLIST FxResourcesTranslated
    )
/*++
 
  Routine Description:

    This routine unmaps the SPB controller register structure.

  Arguments:

    FxDevice - a handle to the framework device object
    FxResourcesRaw - list of translated hardware resources that 
        the PnP manager has assigned to the device
    FxResourcesTranslated - list of raw hardware resources that 
        the PnP manager has assigned to the device

  Return Value:

    Status

--*/
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    PPBC_DEVICE pDevice = GetDeviceContext(FxDevice);
    NT_ASSERT(pDevice != NULL);
    
    NTSTATUS status = STATUS_SUCCESS;
    
    UNREFERENCED_PARAMETER(FxResourcesTranslated);
    
    if (pDevice->pSPIRegisters != NULL)
    {
        MmUnmapIoSpace(pDevice->pSPIRegisters, pDevice->SPIRegistersCb);
        pDevice->pSPIRegisters = NULL;
        pDevice->SPIRegistersCb = 0;
    }

    FuncExit(TRACE_FLAG_WDFLOADING);

    return status;
}

_Use_decl_annotations_
NTSTATUS
OnD0Entry(
    WDFDEVICE FxDevice,
    WDF_POWER_DEVICE_STATE FxPreviousState
    )
/*++
 
  Routine Description:

    This routine allocates objects needed by the driver 
    and initializes the controller hardware.

  Arguments:

    FxDevice - a handle to the framework device object
    FxPreviousState - previous power state

  Return Value:

    Status

--*/
{
    FuncEntry(TRACE_FLAG_WDFLOADING);
    
    PPBC_DEVICE pDevice = GetDeviceContext(FxDevice);
    NT_ASSERT(pDevice != NULL);
    
    UNREFERENCED_PARAMETER(FxPreviousState);

    //
    // Initialize controller.
    //

    pDevice->pCurrentTarget = NULL;
    pDevice->Locked = FALSE;

    ControllerInitialize(pDevice);
    
    FuncExit(TRACE_FLAG_WDFLOADING);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
OnD0Exit(
    WDFDEVICE FxDevice,
    WDF_POWER_DEVICE_STATE FxPreviousState
    )
/*++
 
  Routine Description:

    This routine destroys objects needed by the driver 
    and uninitializes the controller hardware.

  Arguments:

    FxDevice - a handle to the framework device object
    FxPreviousState - previous power state

  Return Value:

    Status

--*/
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    PPBC_DEVICE pDevice = GetDeviceContext(FxDevice);
    NT_ASSERT(pDevice != NULL);
    
    NTSTATUS status = STATUS_SUCCESS;
    
    UNREFERENCED_PARAMETER(FxPreviousState);

    //
    // Uninitialize controller.
    //

    ControllerUninitialize(pDevice);

    pDevice->pCurrentTarget = NULL;
    pDevice->Locked = FALSE;

    FuncExit(TRACE_FLAG_WDFLOADING);

    return status;
}

_Use_decl_annotations_
NTSTATUS
OnSelfManagedIoInit(
    WDFDEVICE FxDevice
    )
/*++
 
  Routine Description:

    Initializes and starts the device's self-managed I/O operations.

  Arguments:
  
    FxDevice - a handle to the framework device object

  Return Value:

    None

--*/
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    PPBC_DEVICE pDevice = GetDeviceContext(FxDevice);
    NTSTATUS status;

    // 
    // Register for monitor power setting callback. This will be
    // used to dynamically set the idle timeout delay according
    // to the monitor power state.
    // 

    NT_ASSERT(pDevice->pMonitorPowerSettingHandle == NULL);
    
    status = PoRegisterPowerSettingCallback(
        WdfDeviceWdmGetDeviceObject(pDevice->FxDevice), 
        &GUID_MONITOR_POWER_ON,
        OnMonitorPowerSettingCallback, 
        pDevice->FxDevice, 
        &pDevice->pMonitorPowerSettingHandle);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_WDFLOADING,
            "Failed to register monitor power setting callback - %!STATUS!",
            status);
                
        goto exit;
    }

exit:

    FuncExit(TRACE_FLAG_WDFLOADING);

    return status;
}

_Use_decl_annotations_
VOID
OnSelfManagedIoCleanup(
    WDFDEVICE FxDevice
    )
/*++
 
  Routine Description:

    Cleanup for the device's self-managed I/O operations.

  Arguments:
  
    FxDevice - a handle to the framework device object

  Return Value:

    None

--*/
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    PPBC_DEVICE pDevice = GetDeviceContext(FxDevice);

    //
    // Unregister for monitor power setting callback.
    //

    if (pDevice->pMonitorPowerSettingHandle != NULL)
    {
        PoUnregisterPowerSettingCallback(pDevice->pMonitorPowerSettingHandle);
        pDevice->pMonitorPowerSettingHandle = NULL;
    }

    FuncExit(TRACE_FLAG_WDFLOADING);
}

_Use_decl_annotations_
NTSTATUS
OnMonitorPowerSettingCallback(
    LPCGUID SettingGuid,
    PVOID Value,
    ULONG ValueLength,
    PVOID Context
   )
/*++
 
  Routine Description:

    This routine updates the idle timeout delay according 
    to the current monitor power setting.

  Arguments:

    SettingGuid - the setting GUID
    Value - pointer to the new value of the power setting that changed
    ValueLength - value of type ULONG that specifies the size, in bytes, 
                  of the new power setting value 
    Context - the WDFDEVICE pointer context

  Return Value:

    Status

--*/
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    UNREFERENCED_PARAMETER(ValueLength);

    WDFDEVICE Device;
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
    BOOLEAN isMonitorOff;
    NTSTATUS status = STATUS_SUCCESS;

    if (Context == NULL)
    {
        status = STATUS_INVALID_PARAMETER;

        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_WDFLOADING,
            "%!FUNC! parameter Context is NULL - %!STATUS!",
            status);

        goto exit;
    }

    Device = (WDFDEVICE)Context;

    //
    // We only expect GUID_MONITOR_POWER_ON notifications
    // in this callback, but let's check just to be sure.
    //

    if (IsEqualGUID(*SettingGuid, GUID_MONITOR_POWER_ON))
    {
        NT_ASSERT(Value != NULL);
        NT_ASSERT(ValueLength == sizeof(ULONG));

        //
        // Determine power setting.
        //

        isMonitorOff = ((*(PULONG)Value) == MONITOR_POWER_OFF);

        //
        // Update the idle timeout delay.
        //

        WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(
            &idleSettings, 
            IdleCannotWakeFromS0);

        idleSettings.IdleTimeoutType = SystemManagedIdleTimeoutWithHint;

        if (isMonitorOff)
        { 
            idleSettings.IdleTimeout = IDLE_TIMEOUT_MONITOR_OFF;
        }
        else
        {
            idleSettings.IdleTimeout = IDLE_TIMEOUT_MONITOR_ON;
 
        }

        status = WdfDeviceAssignS0IdleSettings(
            Device, 
            &idleSettings);

        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_WDFLOADING,
                "Failed to assign S0 idle settings - %!STATUS!",
                status);
                
            goto exit;
        }
    }

exit:
    
    FuncExit(TRACE_FLAG_WDFLOADING);
 
    return status;
}

_Use_decl_annotations_
NTSTATUS
OnTargetConnect(
    WDFDEVICE SpbController,
    SPBTARGET SpbTarget
    )
/*++
 
  Routine Description:

    This routine is invoked whenever a peripheral driver opens
    a target.  It retrieves target-specific settings from the
    Resource Hub and saves them in the target's context.

  Arguments:

    SpbController - a handle to the framework device object
        representing an SPB controller
    SpbTarget - a handle to the SPBTARGET object

  Return Value:

    Status

--*/
{
    FuncEntry(TRACE_FLAG_SPBDDI);

    PPBC_DEVICE pDevice  = GetDeviceContext(SpbController);
    PPBC_TARGET pTarget  = GetTargetContext(SpbTarget);
    
    NT_ASSERT(pDevice != NULL);
    NT_ASSERT(pTarget != NULL);
    
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Get target connection parameters.
    //

    SPB_CONNECTION_PARAMETERS params;
    SPB_CONNECTION_PARAMETERS_INIT(&params);

    SpbTargetGetConnectionParameters(SpbTarget, &params);

    //
    // Retrieve target settings.
    //

    status = PbcTargetGetSettings(pDevice,
                                  params.ConnectionParameters,
                                  &pTarget->Settings
                                  );
    
    //
    // fail on unsupported target settings
    //
    if (pTarget->Settings.DataBitLength != BCM_SPI_DATA_BIT_LENGTH_SUPPORTED ||
        pTarget->Settings.DeviceSelection >= BCM_SPI_CS_SUPPORTED ||
        (pTarget->Settings.GeneralFlags & SPI_SLV_BIT) != 0 ||  
        (pTarget->Settings.TypeSpecificFlags & SPI_WIREMODE_BIT) != 0
        )
    {
        status = STATUS_INVALID_PARAMETER;
    }

    //
    // Initialize target context.
    //

    if (NT_SUCCESS(status))
    {
        pTarget->SpbTarget = SpbTarget;
        pTarget->pCurrentRequest = NULL;

        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_SPBDDI,
            "Connected to SPBTARGET %p at device 0x%lx from WDFDEVICE %p",
            pTarget->SpbTarget,
            pTarget->Settings.DeviceSelection,
            pDevice->FxDevice);
    }
    
    FuncExit(TRACE_FLAG_SPBDDI);

    return status;
}

_Use_decl_annotations_
VOID
OnControllerLock(
    WDFDEVICE SpbController,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest
    )
/*++
 
  Routine Description:

    This routine is invoked whenever the controller is to
    be locked for a single target. The request is only completed
    if there is an error configuring the transfer.

  Arguments:

    SpbController - a handle to the framework device object
        representing an SPB controller
    SpbTarget - a handle to the SPBTARGET object
    SpbRequest - a handle to the SPBREQUEST object

  Return Value:

    None.  The request is completed synchronously.

--*/
{
    FuncEntry(TRACE_FLAG_SPBDDI);

    PPBC_DEVICE  pDevice  = GetDeviceContext(SpbController);
    PPBC_TARGET  pTarget  = GetTargetContext(SpbTarget);
    
    NT_ASSERT(pDevice  != NULL);
    NT_ASSERT(pTarget  != NULL);

    //
    // Acquire the device lock.
    //

    WdfSpinLockAcquire(pDevice->Lock);

    //
    // Assign current target.
    //

    NT_ASSERT(pDevice->pCurrentTarget == NULL);
    NT_ASSERT(!pDevice->Locked);

    pDevice->pCurrentTarget = pTarget;
    pDevice->Locked = TRUE;

    WdfSpinLockRelease(pDevice->Lock);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_SPBDDI,
        "Controller locked for SPBTARGET %p at device 0x%lx (WDFDEVICE %p)",
        pTarget->SpbTarget,
        pTarget->Settings.DeviceSelection,
        pDevice->FxDevice);

    //
    // Complete lock request.
    //

    SpbRequestComplete(SpbRequest, STATUS_SUCCESS);
   
    FuncExit(TRACE_FLAG_SPBDDI);
}

_Use_decl_annotations_
VOID
OnControllerUnlock(
    WDFDEVICE SpbController,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest
    )
/*++
 
  Routine Description:

    This routine is invoked whenever the controller is to
    be unlocked for a single target. The request is only completed
    if there is an error configuring the transfer.

  Arguments:

    SpbController - a handle to the framework device object
        representing an SPB controller
    SpbTarget - a handle to the SPBTARGET object
    SpbRequest - a handle to the SPBREQUEST object

  Return Value:

    None.  The request is completed asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_SPBDDI);

    PPBC_DEVICE  pDevice  = GetDeviceContext(SpbController);
    PPBC_TARGET  pTarget  = GetTargetContext(SpbTarget);
    
    NT_ASSERT(pDevice  != NULL);
    NT_ASSERT(pTarget  != NULL);

    //
    // Acquire the device lock.
    //

    WdfSpinLockAcquire(pDevice->Lock);

    ControllerUnlockTransfer(pDevice);

    //
    // Remove current target.
    //

    NT_ASSERT(pDevice->pCurrentTarget == pTarget);
    NT_ASSERT(pDevice->Locked);

    pDevice->pCurrentTarget = NULL;
    pDevice->Locked = FALSE;
    
    WdfSpinLockRelease(pDevice->Lock);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_SPBDDI,
        "Controller unlocked for SPBTARGET %p at device 0x%lx (WDFDEVICE %p)",
        pTarget->SpbTarget,
        pTarget->Settings.DeviceSelection,
        pDevice->FxDevice);

    //
    // Complete lock request.
    //

    SpbRequestComplete(SpbRequest, STATUS_SUCCESS);
    
    FuncExit(TRACE_FLAG_SPBDDI);
}

_Use_decl_annotations_
VOID
OnRead(
    WDFDEVICE SpbController,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest,
    size_t Length
    )
/*++
 
  Routine Description:

    This routine sets up a read from the target device using
    the supplied buffers.  The request is only completed
    if there is an error configuring the transfer.

  Arguments:

    SpbController - a handle to the framework device object
        representing an SPB controller
    SpbTarget - a handle to the SPBTARGET object
    SpbRequest - a handle to the SPBREQUEST object
    Length - the number of bytes to read from the target

  Return Value:

    None.  The request is completed asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_SPBDDI);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_SPBDDI,
        "Received read request %p of length %Iu for SPBTARGET %p (WDFDEVICE %p)",
        SpbRequest,
        Length,
        SpbTarget,
        SpbController);

    OnNonSequenceRequest(
        SpbController,
        SpbTarget,
        SpbRequest,
        Length);

    FuncExit(TRACE_FLAG_SPBDDI);
}

_Use_decl_annotations_
VOID
OnWrite(
    WDFDEVICE SpbController,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest,
    size_t Length
    )
/*++
 
  Routine Description:

    This routine sets up a write to the target device using
    the supplied buffers.  The request is only completed
    if there is an error configuring the transfer.

  Arguments:

    SpbController - a handle to the framework device object
        representing an SPB controller
    SpbTarget - a handle to the SPBTARGET object
    SpbRequest - a handle to the SPBREQUEST object
    Length - the number of bytes to write to the target

  Return Value:

    None.  The request is completed asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_SPBDDI);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_SPBDDI,
        "Received write request %p of length %Iu for SPBTARGET %p (WDFDEVICE %p)",
        SpbRequest,
        Length,
        SpbTarget,
        SpbController);

    OnNonSequenceRequest(
        SpbController,
        SpbTarget,
        SpbRequest,
        Length);

    FuncExit(TRACE_FLAG_SPBDDI);
}


_Use_decl_annotations_
VOID
OnNonSequenceRequest(
    WDFDEVICE SpbController,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest,
    size_t Length
    )
    /*++

    Routine Description:

    This is a helper routine used to configure
    the request context and controller hardware for a non-
    sequence SPB request. It validates parameters and retrieves
    the transfer buffer as necessary.

    Arguments:

    SpbController - a handle to the framework device object
    representing an SPB controller
    SpbTarget - a handle to the SPBTARGET object
    SpbRequest - a handle to the SPBREQUEST object
    Length - the number of bytes to write/read to/from the target

    Return Value:

    None.  The request is completed asynchronously.

    --*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    PPBC_DEVICE pDevice = GetDeviceContext(SpbController);
    PPBC_TARGET pTarget = GetTargetContext(SpbTarget);
    PPBC_REQUEST pRequest = GetRequestContext(SpbRequest);

    UNREFERENCED_PARAMETER(Length);

    NTSTATUS status;

    //
    // Get the request parameters.
    //

    SPB_REQUEST_PARAMETERS params;
    SPB_REQUEST_PARAMETERS_INIT(&params);
    SpbRequestGetParameters(SpbRequest, &params);

    //
    // Initialize request context with info that persist
    // the lifetime of the request
    //

    pRequest->SpbRequest = SpbRequest;
    pRequest->Type = params.Type;
    pRequest->CurrentTransferSequencePosition = params.Position;
    pRequest->TransferCount = 1;
    pRequest->CurrentTransferIndex = 0;
    pRequest->TotalInformation = 0;
    pRequest->RequestLength = params.Length;

    status = OnRequest(pDevice, pTarget, pRequest);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_SPBDDI,
            "Error configuring non-sequence, completing SPBREQUEST %p synchronously - %!STATUS!",
            pRequest->SpbRequest,
            status);

        SpbRequestComplete(
            pRequest->SpbRequest,
            status);
    }

    FuncExit(TRACE_FLAG_TRANSFER);
}

_Use_decl_annotations_
VOID
OnSequenceRequest(
    WDFDEVICE SpbController,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest,
    ULONG /*TransferCount*/
    )
/*++
 
  Routine Description:

    This routine sets up a sequence of reads and writes.  It 
    validates parameters as necessary.  The request is only 
    completed if there is an error configuring the transfer.

  Arguments:

    SpbController - a handle to the framework device object
        representing an SPB controller
    SpbTarget - a handle to the SPBTARGET object
    SpbRequest - a handle to the SPBREQUEST object
    TransferCount - number of individual transfers in the sequence

  Return Value:

    None.  The request is completed asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_SPBDDI);

    PPBC_DEVICE pDevice = GetDeviceContext(SpbController);
    PPBC_TARGET pTarget = GetTargetContext(SpbTarget);
    PPBC_REQUEST pRequest = GetRequestContext(SpbRequest);
    
    NTSTATUS status = STATUS_SUCCESS;
    
    //
    // Get request parameters.
    //

    SPB_REQUEST_PARAMETERS params;
    SPB_REQUEST_PARAMETERS_INIT(&params);
    SpbRequestGetParameters(SpbRequest, &params);
    
    //
    // Initialize request context.
    //
    
    pRequest->SpbRequest = SpbRequest;
    pRequest->Type = params.Type;
    pRequest->CurrentTransferSequencePosition = params.Position;
    pRequest->CurrentTransferIndex = 0;
    pRequest->TotalInformation = 0;
    pRequest->RequestLength = params.Length;
    pRequest->TransferCount = params.SequenceTransferCount;

    //
    // Special handling for fullduplex transfer
    //

    if (params.Type == SpbRequestTypeOther)
    {   
        //
        // Fullduplex request is a special kind of sequence request
        // It comes as sequence of write then read transfer and SPB
        // assign to it different rquest type
        //

        if (pRequest->TransferCount != 2)
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_SPBDDI,
                "Full-duplex request should specify only 2 transfers, %lu specified. SPBREQUEST %p (SPBTARGET %p)",
                pRequest->TransferCount,
                pRequest->SpbRequest,
                SpbTarget);

            status = STATUS_INVALID_PARAMETER;
            goto exit;
        }


        pRequest->TransferCount = 1;

        //
        // check for supported fullduplex sequences and the lock / unlock case
        //

        if (!pDevice->Locked)
        {
            NT_ASSERT(params.Position == SpbRequestSequencePositionSingle);
        }
        else
        {
            NT_ASSERT(params.Position == SpbRequestSequencePositionFirst ||
                      params.Position == SpbRequestSequencePositionContinue);
        }

        status = PbcRequestSetNthTransferInfo(pRequest, 1);
        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_SPBDDI,
                "Error configuring full-duplex request context for SPBREQUEST %p (SPBTARGET %p) - %!STATUS!",
                pRequest->SpbRequest,
                SpbTarget,
                status);

            goto exit;
        }

        if (pRequest->CurrentTransferDirection != SpbTransferDirectionFromDevice)
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_SPBDDI,
                "Full-duplex request 2nd transfer should be a read transfer. SPBREQUEST %p (SPBTARGET %p)",
                pRequest->SpbRequest,
                SpbTarget);

            status = STATUS_INVALID_PARAMETER;
            goto exit;
        }

        if (pRequest->CurrentTransferDelayInUs > 0)
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_SPBDDI,
                "Full-duplex request SPBREQUEST %p should have zero delay specified (SPBTARGET %p)",
                pRequest->SpbRequest,
                SpbTarget);

            status = STATUS_INVALID_PARAMETER;
            goto exit;
        }

        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_SPBDDI,
            "Received full-duplex for SPBTARGET %p (WDFDEVICE %p)",
            SpbTarget,
            SpbController);
    }
    else
    {
        NT_ASSERT(params.Position == SpbRequestSequencePositionSingle);
        NT_ASSERT(params.Type == SpbRequestTypeSequence);

        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_SPBDDI,
            "Received sequence request with transfer count %d for SPBTARGET %p (WDFDEVICE %p)",
            pRequest->TransferCount,
            SpbTarget,
            SpbController);
    }

    //
    // Configure the request.
    //

    //
    // Get length and MDL for first transfer in the request
    // it will be the write request if request is fullduplex
    //

    status = PbcRequestSetNthTransferInfo(pRequest, 0);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR, 
            TRACE_FLAG_SPBDDI, 
            "Error configuring request context for SPBREQUEST %p (SPBTARGET %p) - %!STATUS!",
            pRequest->SpbRequest,
            SpbTarget,
            status);

        goto exit;
    }

    if (pRequest->Type == SpbRequestTypeOther)
    {
        if (pRequest->CurrentTransferDirection != SpbTransferDirectionToDevice)
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_SPBDDI,
                "Full-duplex request 1st transfer should be a write transfer. SPBREQUEST %p (SPBTARGET %p)",
                pRequest->SpbRequest,
                SpbTarget);

            status = STATUS_INVALID_PARAMETER;
            goto exit;
        }

        if (pRequest->CurrentTransferDelayInUs > 0)
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_SPBDDI,
                "Full-duplex request SPBREQUEST %p should have zero delay specified (SPBTARGET %p)",
                pRequest->SpbRequest,
                SpbTarget);

            status = STATUS_INVALID_PARAMETER;
            goto exit;
        }

        NT_ASSERT(pRequest->CurrentTransferDirection == SpbTransferDirectionToDevice);

        pRequest->CurrentTransferDirection = SpbTransferDirectionNone;

        //
        // fullduplex request actual transfer length .ie the amount of bytes that goes
        // over the wires is the max of write and read transfers supplied by SPB
        //

        pRequest->RequestLength = max(
            pRequest->CurrentTransferWriteLength,
            pRequest->CurrentTransferReadLength);
    }

    status = OnRequest(pDevice, pTarget, pRequest);

exit:

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_SPBDDI,
            "Error configuring sequence, completing SPBREQUEST %p synchronously - %!STATUS!", 
            pRequest->SpbRequest,
            status);

        SpbRequestComplete(
            pRequest->SpbRequest,
            status);
    }

    FuncExit(TRACE_FLAG_SPBDDI);
}

_Use_decl_annotations_
VOID
OnOtherInCallerContext(
    WDFDEVICE SpbController,
    WDFREQUEST FxRequest
    )
/*++
 
  Routine Description:

    This routine preprocesses custom IO requests before the framework
    places them in an IO queue. For requests using the SPB transfer list
    format, it calls SpbRequestCaptureIoOtherTransferList to capture the
    client's buffers.

  Arguments:

    SpbController - a handle to the framework device object
        representing an SPB controller
    SpbRequest - a handle to the SPBREQUEST object

  Return Value:

    None.  The request is either completed or enqueued asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_SPBDDI);

    NTSTATUS status = STATUS_SUCCESS;

    //
    // Check for custom IOCTLs that this driver handles. If
    // unrecognized mark as STATUS_NOT_SUPPORTED and complete.
    //

    WDF_REQUEST_PARAMETERS fxParams;
    WDF_REQUEST_PARAMETERS_INIT(&fxParams);

    WdfRequestGetParameters(FxRequest, &fxParams);

    if ((fxParams.Type != WdfRequestTypeDeviceControl) &&
        (fxParams.Type != WdfRequestTypeDeviceControlInternal))
    {
        status = STATUS_NOT_SUPPORTED;
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_SPBDDI,
            "FxRequest %p is of unsupported request type - %!STATUS!",
            FxRequest,
            status
            );
        goto exit;
    }

    SpbIoctl controlCode;
    controlCode = (SpbIoctl)
        fxParams.Parameters.DeviceIoControl.IoControlCode;

    switch (controlCode)
    {
    case IOCTL_SPB_FULL_DUPLEX:
        break;
    default:
        status = STATUS_NOT_SUPPORTED;
        goto exit;
    }

    //
    // For custom IOCTLs that use the SPB transfer list format
    // (i.e. sequence formatting), call SpbRequestCaptureIoOtherTransferList
    // so that the driver can leverage other SPB DDIs for this request.
    //

    status = SpbRequestCaptureIoOtherTransferList((SPBREQUEST)FxRequest);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_SPBDDI,
            "Failed to capture transfer list for custom SpbRequest %p - %!STATUS!",
            FxRequest,
            status
            );
        goto exit;
    }

    //
    // Preprocessing has succeeded, enqueue the request.
    //

    status = WdfDeviceEnqueueRequest(SpbController, FxRequest);

    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

exit:

    if (!NT_SUCCESS(status))
    {
        WdfRequestComplete(FxRequest, status);
    }
    
    FuncExit(TRACE_FLAG_SPBDDI);
}

_Use_decl_annotations_
VOID
OnOther(
    WDFDEVICE SpbController,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
    )
/*++
 
  Routine Description:

    This routine processes custom IO requests that are not natively
    supported by the SPB framework extension. For requests using the 
    SPB transfer list format, SpbRequestCaptureIoOtherTransferList 
    must have been called in the driver's OnOtherInCallerContext routine.

  Arguments:

    SpbController - a handle to the framework device object
        representing an SPB controller
    SpbTarget - a handle to the SPBTARGET object
    SpbRequest - a handle to the SPBREQUEST object
    OutputBufferLength - the request's output buffer length
    InputBufferLength - the requests input buffer length
    IoControlCode - the device IO control code

  Return Value:

    None.  The request is completed asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_SPBDDI);
    
    NTSTATUS status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);


    if (IoControlCode == IOCTL_SPB_FULL_DUPLEX)
    {
        OnSequenceRequest(
            SpbController,
            SpbTarget,
            SpbRequest,
            2 // FullDuplex is formatted as 1 write follwed by 1 read transfer
            );
    }
    else
    {
        status = STATUS_NOT_SUPPORTED;
    }

    if (!NT_SUCCESS(status))
    {
        SpbRequestComplete(SpbRequest, status);
    }
    
    FuncExit(TRACE_FLAG_SPBDDI);
}

/////////////////////////////////////////////////
//
// PBC functions.
//
/////////////////////////////////////////////////

_Use_decl_annotations_
NTSTATUS
PbcTargetGetSettings(
    PPBC_DEVICE pDevice,
    PVOID ConnectionParameters,
    PPBC_TARGET_SETTINGS  pSettings
    )
/*++
 
  Routine Description:

    This routine populates the target's settings.

  Arguments:

    pDevice - a pointer to the PBC device context
    ConnectionParameters - a pointer to a blob containing the 
        connection parameters
    Settings - a pointer the the target's settings

  Return Value:

    Status

--*/
{
    FuncEntry(TRACE_FLAG_PBCLOADING);

    UNREFERENCED_PARAMETER(pDevice);

    NT_ASSERT(ConnectionParameters != nullptr);
    NT_ASSERT(pSettings != nullptr);

    PRH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER connection;
    PPNP_SERIAL_BUS_DESCRIPTOR descriptor;
    PPNP_SPI_SERIAL_BUS_DESCRIPTOR spiDescriptor;

    connection = (PRH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER)
        ConnectionParameters;

    if (connection->PropertiesLength < sizeof(PNP_SPI_SERIAL_BUS_DESCRIPTOR))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_PBCLOADING,
            "Invalid connection properties (length = %lu, expected = %Iu)",
            connection->PropertiesLength,
            sizeof(PNP_SPI_SERIAL_BUS_DESCRIPTOR));

        return STATUS_INVALID_PARAMETER;
    }

    descriptor = (PPNP_SERIAL_BUS_DESCRIPTOR)
        connection->ConnectionProperties;

    if (descriptor->SerialBusType != SPI_SERIAL_BUS_TYPE)
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_PBCLOADING,
            "Bus type %c not supported, only SPI",
            descriptor->SerialBusType);

        return STATUS_INVALID_PARAMETER;
    }

    spiDescriptor = (PPNP_SPI_SERIAL_BUS_DESCRIPTOR)
        connection->ConnectionProperties;

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_PBCLOADING,
        "SPI Connection Descriptor %p ConnectionSpeed:%lu ",
        spiDescriptor,
        spiDescriptor->ConnectionSpeed
        );

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_PBCLOADING,
        "    Phase:%u Polarity:%u DeviceSelection:0x%hx",
        spiDescriptor->Phase,
        spiDescriptor->Polarity,
        spiDescriptor->DeviceSelection
        );

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_PBCLOADING,
        "    WireMode:%u wires DevicePolarity:%u ",
        (descriptor->TypeSpecificFlags & SPI_WIREMODE_BIT) ? 3 : 4,
        (descriptor->TypeSpecificFlags & SPI_DEVICEPOLARITY_BIT) ? 1 : 0
        );

    // Target settings for transaction
    pSettings->TypeSpecificFlags = descriptor->TypeSpecificFlags;
    pSettings->GeneralFlags = descriptor->GeneralFlags;
    pSettings->ConnectionSpeed = spiDescriptor->ConnectionSpeed;
    pSettings->DataBitLength = spiDescriptor->DataBitLength;
    pSettings->Phase = spiDescriptor->Phase;
    pSettings->Polarity = spiDescriptor->Polarity;
    pSettings->DeviceSelection = spiDescriptor->DeviceSelection;

    FuncExit(TRACE_FLAG_PBCLOADING);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
PbcRequestSetNthTransferInfo(
    PPBC_REQUEST  pRequest,
    ULONG TransferIndex
    )
/*++
 
  Routine Description:

    This is a helper routine used to configure the request
    context and controller hardware for a transfer within a 
    sequence. It validates parameters and retrieves
    the transfer buffer as necessary.

  Arguments:

    pRequest - a pointer to the PBC request context
    Index - index of the transfer within the sequence

  Return Value:

    STATUS

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NT_ASSERT(pRequest != NULL);
 
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Get transfer parameters for index.
    //

    SPB_TRANSFER_DESCRIPTOR descriptor;
    PMDL pMdl;
    
    SPB_TRANSFER_DESCRIPTOR_INIT(&descriptor);

    SpbRequestGetTransferParameters(
        pRequest->SpbRequest, 
        TransferIndex,
        &descriptor, 
        &pMdl);
       
    NT_ASSERT(pMdl != NULL);
    
    //
    // Configure request context.
    //

    pRequest->CurrentTransferInformation  = 0;
    pRequest->CurrentTransferDirection = descriptor.Direction;
    pRequest->CurrentTransferDelayInUs = descriptor.DelayInUs;

    // 
    // This method is called twice in preparation for the fullduplex
    // transfer in which both write and read transfer info is fetched.
    // For other types, this method is called once after each transfer
    // 

    if (pRequest->Type != SpbRequestTypeOther)
    {
        pRequest->CurrentTransferReadLength = 0;
        pRequest->CurrentTransferWriteLength = 0;
    }

    if (pRequest->CurrentTransferDirection == SpbTransferDirectionFromDevice)
    {
        pRequest->pCurrentTransferReadMdlChain = pMdl;
        pRequest->CurrentTransferReadLength = descriptor.TransferLength;
    }
    else if (pRequest->CurrentTransferDirection == SpbTransferDirectionToDevice)
    {
        pRequest->pCurrentTransferWriteMdlChain = pMdl;
        pRequest->CurrentTransferWriteLength = descriptor.TransferLength;
    }
    else
    {
        NT_ASSERT(!"Transfer should either To or From device");
    }

    //
    // Update sequence position if request is type sequence.
    //

    if (pRequest->Type == SpbRequestTypeSequence)
    {
        if (pRequest->TransferCount == 1)
        {
            pRequest->CurrentTransferSequencePosition = SpbRequestSequencePositionSingle;
        }
        else if (TransferIndex == 0)
        {
            pRequest->CurrentTransferSequencePosition = SpbRequestSequencePositionFirst;
        }
        else if (TransferIndex == (pRequest->TransferCount - 1))
        {
            pRequest->CurrentTransferSequencePosition = SpbRequestSequencePositionLast;
        }
        else
        {
            pRequest->CurrentTransferSequencePosition = SpbRequestSequencePositionContinue;
        }
    }

    FuncExit(TRACE_FLAG_TRANSFER);

    return status;
}

_Use_decl_annotations_
NTSTATUS
OnRequest(
    PPBC_DEVICE pDevice,
    PPBC_TARGET pTarget,
    PPBC_REQUEST pRequest
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    WdfSpinLockAcquire(pDevice->Lock);

    //
    // Update device and target contexts.
    //

    if (pDevice->Locked)
    {
        NT_ASSERT(pDevice->pCurrentTarget == pTarget);
    }
    else
    {
        NT_ASSERT(pDevice->pCurrentTarget == NULL);
        pDevice->pCurrentTarget = pTarget;
    }

    NT_ASSERT(pTarget->pCurrentRequest == NULL);
    pTarget->pCurrentRequest = pRequest;

    (void)KeSetEvent(
        &pDevice->TransferThreadWakeEvt,
        0,
        FALSE);

    WdfSpinLockRelease(pDevice->Lock);

    return status;
}

_Use_decl_annotations_
VOID
OnRequestPollMode(
    PPBC_DEVICE pDevice
    )
{
    PPBC_REQUEST pRequest = pDevice->pCurrentTarget->pCurrentRequest;

#if DBG
    ULONGLONG requestTimeNoDelayUs = ControllerEstimateRequestCompletionTimeUs(pDevice->pCurrentTarget, pRequest, false);
    ULONGLONG requestTimeWithDelayUs = ControllerEstimateRequestCompletionTimeUs(pDevice->pCurrentTarget, pRequest, true);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Controller estimated request time to be %I64u us for %Iu bytes, with %I64u us spent in delays (SPBREQUEST %p, WDFDEVICE %p)",
        requestTimeWithDelayUs,
        pRequest->RequestLength,
        requestTimeWithDelayUs - requestTimeNoDelayUs,
        pRequest->SpbRequest,
        pDevice->FxDevice);
#endif

    //
    // Configure controller HW if necessary and kick-off transfer
    //

    if (pRequest->CurrentTransferSequencePosition == SpbRequestSequencePositionSingle ||
        pRequest->CurrentTransferSequencePosition == SpbRequestSequencePositionFirst)
    {
        ControllerConfigForTargetAndActivate(pDevice);
    }

    NTSTATUS status = STATUS_SUCCESS;
    bool bIsRequestComplete = false;

    // 
    // Fulduplex request despite consisting of 2 transfers write followed
    // by read, we treat the request as 1 transfer in which write and read
    // transfers happen at the same time in fullduplex manner.
    //

    if (pRequest->Type == SpbRequestTypeOther)
    {
        status = ControllerDoOneTransferPollMode(pDevice, pRequest);

        bIsRequestComplete = ControllerCompleteTransfer(pDevice, pRequest, status);
        NT_ASSERT(bIsRequestComplete);
    }
    else
    {
        do
        {
            status = PbcRequestSetNthTransferInfo(pRequest, pRequest->CurrentTransferIndex);
            if (NT_SUCCESS(status))
            {
                status = ControllerDoOneTransferPollMode(pDevice, pRequest);
            }

            bIsRequestComplete = ControllerCompleteTransfer(pDevice, pRequest, status);
        } while (!bIsRequestComplete);
    }
}

_Use_decl_annotations_
VOID
TransferPollModeThread(
    PVOID StartContext
    )
{
    FuncEntry(TRACE_FLAG_TRANSFER);
    
    PPBC_DEVICE pDevice = GetDeviceContext((WDFDEVICE)StartContext);



    //
    // Set thread affinity mask to allow rescheduling the current thread
    // on any processor but CPU0. Purpose is to move polling to any thread
    // other than the system main thread on which interupts are being
    // handled to make polling smooth and uninterruptable as possible
    //

    NT_ASSERTMSG("IRQL unexpected", KeGetCurrentIrql() < DISPATCH_LEVEL);
    ULONG numCpus = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    ULONG noCpu0AffinityMask = (~(ULONG(~0x0) << numCpus) & ULONG(~0x1));
    KAFFINITY callerAffinity = KeSetSystemAffinityThreadEx(KAFFINITY(noCpu0AffinityMask));
    NT_ASSERTMSG("Affinity not set as asked", KeGetCurrentProcessorNumberEx(NULL) != 0);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Transfer poll mode thread started on processor %u. WDFDEVICE %p",
        KeGetCurrentProcessorNumberEx(NULL),
        pDevice->FxDevice);

    NTSTATUS status;

    for (;;)
    {
        //
        // Wait until waken up to either shutdown or 
        // handle a request transfer
        //

        status = KeWaitForSingleObject(
            &pDevice->TransferThreadWakeEvt,
            Executive,
            KernelMode,
            FALSE,
            nullptr);
        NT_ASSERTMSG(
            "KeWaitForSingleObject non-success wake reason is not possible",
            status == STATUS_SUCCESS);

        if (InterlockedOr(&pDevice->TransferThreadShutdown, 0))
            break;

        OnRequestPollMode(pDevice);
    }

    KeRevertToUserAffinityThreadEx(callerAffinity);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Transfer poll mode thread shutting down. WDFDEVICE %p",
        pDevice->FxDevice);

    FuncExit(TRACE_FLAG_TRANSFER);
}