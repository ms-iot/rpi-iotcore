// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//   openclos.c
//
// Abstract:
//
//    This module contains the code that is very specific to
//    opening, closing, and cleaning up in the serial driver.

#include "precomp.h"
#include "openclos.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGESER,SerialGetCharTime)
#pragma alloc_text(PAGESER,SerialEvtFileClose)
#pragma alloc_text(PAGESER,SerialDrainUART)
#pragma alloc_text(PAGESRP0,SerialEvtDeviceFileCreate)
#pragma alloc_text(PAGESRP0,SerialCreateTimersAndDpcs)
#endif

/*++

Routine Description:

    The framework calls a driver's EvtDeviceFileCreate callback
    when the framework receives an IRP_MJ_CREATE request.
    The system sends this request when a user application opens the
    device to perform an I/O operation, such as reading or writing a file.
    This callback is called synchronously, in the context of the thread
    that created the IRP_MJ_CREATE request.

Arguments:

    Device - Handle to a framework device object.
    FileObject - Pointer to fileobject that represents the open handle.
    CreateParams - Copy of the Create IO_STACK_LOCATION

Return Value:

   VOID.

--*/
_Use_decl_annotations_
VOID
SerialEvtDeviceFileCreate (
    WDFDEVICE Device,
    WDFREQUEST Request,
    WDFFILEOBJECT FileObject
    )
{
    NTSTATUS status;
    PSERIAL_DEVICE_EXTENSION extension = SerialGetDeviceExtension (Device);

    UNREFERENCED_PARAMETER(FileObject);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "++SerialEvtDeviceFileCreate(%wZ)\r\n", &extension->DeviceName);

    // If there is a debugger conflict, we should not get here
    // since the device is not exposed. 

    if (extension->DebugPortInUse) {
        ASSERT(FALSE);
        status = STATUS_DEVICE_NOT_READY;
        goto End;
    }

    // Reserve and commit function configuration

    status = SerialReserveFunctionConfig(Device, TRUE);
    if (!NT_SUCCESS(status)) {
        goto End;
    }

    status = SerialDeviceFileCreateWorker(Device);
    if (!NT_SUCCESS(status)) {
        goto End;
    }

    status = STATUS_SUCCESS;

End:

    // Cleanup on failure

    if (!NT_SUCCESS(status)) {
        if ((extension->FunctionConfigHandle != NULL) &&
            !extension->DebugPortInUse) {
            WdfObjectDelete(extension->FunctionConfigHandle);
            extension->FunctionConfigHandle = NULL;
        }
    }

    // Complete the WDF request.

    WdfRequestComplete(Request, status);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "--SerialEvtDeviceFileCreate(%wZ)\r\n",
        &extension->DeviceName);
    return;
}

/*++

Routine Description:

    This is the dispatch routine for IRP_MJ_CREATE. The system sends this
    request when a user application opens the device to perform an I/O
    operation, such as reading or writing a file.

Arguments:

    DeviceObject - Pointer to the device object for this device
    Irp - Pointer to the IRP for the current request

Return Value:

   NT status code

--*/
_Use_decl_annotations_
NTSTATUS
SerialWdmDeviceFileCreate (
    WDFDEVICE Device,
    PIRP Irp
    )
{
    NTSTATUS status;
    PSERIAL_DEVICE_EXTENSION extension = SerialGetDeviceExtension (Device);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                "++SerialWdmDeviceFileCreate(%wZ)\r\n",
                &extension->DeviceName);

    status = SerialDeviceFileCreateWorker(Device);

    // Complete the WDM request.

    Irp->IoStatus.Information = 0L;
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                "--SerialWdmDeviceFileCreate(%wZ)\r\n",
                &extension->DeviceName);
    return status;
}

_Use_decl_annotations_
NTSTATUS
SerialDeviceFileCreateWorker (
    WDFDEVICE Device
    )
{
    NTSTATUS status;
    PSERIAL_DEVICE_EXTENSION extension = SerialGetDeviceExtension (Device);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "++SerialDeviceFileCreateWorker()\r\n");

    // Create a buffer for the RX data when no reads are outstanding.

    extension->InterruptReadBuffer = NULL;
    extension->BufferSize = 0;

    switch (MmQuerySystemSize()) {

        case MmLargeSystem: {

            extension->BufferSize = 4096;
            extension->InterruptReadBuffer = ExAllocatePoolWithTag(NonPagedPool,
                                                                 extension->BufferSize,
                                                                 POOL_TAG);

            if (extension->InterruptReadBuffer) {
                break;
            }

        }

        case MmMediumSystem: {

            extension->BufferSize = 1024;
            extension->InterruptReadBuffer = ExAllocatePoolWithTag(NonPagedPool,
                                                                 extension->BufferSize,
                                                                 POOL_TAG);

            if (extension->InterruptReadBuffer) {
                break;
            }

        }

        case MmSmallSystem: {

            extension->BufferSize = 128;
            extension->InterruptReadBuffer = ExAllocatePoolWithTag(NonPagedPool,
                                                                 extension->BufferSize,
                                                                 POOL_TAG);

        }

    }

    if (!extension->InterruptReadBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // By taking a power reference by calling WdfDeviceStopIdle, we prevent the
    // framework from powering down our device due to idle timeout when there
    // is an open handle.  Power reference also moves the device to D0 if we are
    // idled out. If you fail create anywhere later in this routine, do make sure
    // drop the reference.

    status = WdfDeviceStopIdle(Device, TRUE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // wakeup is not currently enabled

    extension->IsWakeEnabled = FALSE;

    // On a new open we "flush" the read queue by initializing the
    // count of characters.

    extension->CharsInInterruptBuffer = 0;
    extension->LastCharSlot = extension->InterruptReadBuffer +
                              (extension->BufferSize - 1);

    extension->ReadBufferBase = extension->InterruptReadBuffer;
    extension->CurrentCharSlot = extension->InterruptReadBuffer;
    extension->FirstReadableChar = extension->InterruptReadBuffer;

    extension->TotalCharsQueued = 0;

    // We set up the default xon/xoff limits.

    extension->HandFlow.XoffLimit = extension->BufferSize >> 3;
    extension->HandFlow.XonLimit = extension->BufferSize >> 1;

    extension->WmiCommData.XoffXmitThreshold = extension->HandFlow.XoffLimit;
    extension->WmiCommData.XonXmitThreshold = extension->HandFlow.XonLimit;

    extension->BufferSizePt8 = ((3*(extension->BufferSize>>2))+
                                   (extension->BufferSize>>4));

    // Mark the device as busy for WMI

    extension->WmiCommData.IsBusy = TRUE;

    extension->IrpMaskLocation = NULL;
    extension->HistoryMask = 0;
    extension->IsrWaitMask = 0;

    extension->SendXonChar = FALSE;
    extension->SendXoffChar = FALSE;

#if !DBG
    //
    // Clear out the statistics.
    //

    WdfInterruptSynchronize(extension->WdfInterrupt,
                            SerialClearStats,
                            extension);
#endif

    // The escape char replacement must be reset upon every open.

    extension->EscapeChar = 0;

    // We don't want the device to be removed or stopped when there is an handle
    //
    // Note to anyone copying this sample as a starting point:
    //
    // This works in this driver simply because this driver supports exactly
    // one open handle at a time.  If it supported more, then it would need
    // counting logic to determine when all the reasons for failing Stop/Remove
    // were gone.

    WdfDeviceSetStaticStopRemove(Device, FALSE);

    // Synchronize with the ISR and let it know that the device
    // has been successfully opened.

    WdfInterruptSynchronize(extension->WdfInterrupt,
                            SerialMarkOpen,
                            extension);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "--SerialDeviceFileCreateWorker()\r\n");

    return STATUS_SUCCESS;
}

/*++

Routine Description:

    SerialReserveFunctionConfig is called from EvtDeviceFileCreate callback
    when the framework receives an IRP_MJ_CREATE request.
    If the platform supports multiple alternate functions for the miniUART,
    the function opens the associated connection ID, and commits the specific 
    function required for the driver to work properly.
    For example in Raspberry Pi3 a GPIO function configuration routes the RX/TX 
    signals to GPIO 15/14, that are exposed at the board headers.

Arguments:

    Device - Handle to a framework device object.
    IsCommit - If to commit function configuration (TRUE) or just reserve (FALSE).

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS
SerialReserveFunctionConfig (
    WDFDEVICE Device,
    BOOLEAN IsCommit
    )
{
    NTSTATUS status;
    PSERIAL_DEVICE_EXTENSION extension = SerialGetDeviceExtension(Device);
    WDF_OBJECT_ATTRIBUTES wdfObjectAttributes;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    DECLARE_UNICODE_STRING_SIZE(devicePath, RESOURCE_HUB_PATH_CHARS);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, 
                "++SerialReserveFunctionConfig()\r\n");

    ASSERT(extension->FunctionConfigHandle == NULL);

    if (extension->FunctionConfigConnectionId.QuadPart == 0LL) {
        status = STATUS_SUCCESS;
        goto End;
    }

    status = RESOURCE_HUB_CREATE_PATH_FROM_ID(&devicePath,
                extension->FunctionConfigConnectionId.LowPart,
                extension->FunctionConfigConnectionId.HighPart);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_CREATE_CLOSE, 
                    "RESOURCE_HUB_CREATE_PATH_FROM_ID failed "
                    " Err=%Xh\r\n",
                    status);
        goto End;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&wdfObjectAttributes);
    wdfObjectAttributes.ParentObject = extension->WdfDevice;

    status = WdfIoTargetCreate(extension->WdfDevice,
                               &wdfObjectAttributes,
                               &extension->FunctionConfigHandle);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_CREATE_CLOSE,
                    "WdfIoTargetCreate failed Err=%Xh\r\n",
                    status);
        goto End;
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                &devicePath,
                                                FILE_GENERIC_READ | 
                                                 FILE_GENERIC_WRITE);

    status = WdfIoTargetOpen(extension->FunctionConfigHandle, &openParams);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_CREATE_CLOSE,
                    "WdfIoTargetOpen failed. "
                    "status = %Xh, devicePath = %wZ)\r\n",
                    status,
                    &devicePath);
        goto End;
    }

    // If we need to commit the function configuration, send IOCTL to commit 
    // the configuration to H/W.
    
    if (IsCommit) {
        status = WdfIoTargetSendIoctlSynchronously(extension->FunctionConfigHandle,
                                                NULL,
                                                IOCTL_GPIO_COMMIT_FUNCTION_CONFIG_PINS,
                                                NULL,
                                                0,
                                                NULL,
                                                0);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_CREATE_CLOSE,
                        "IOCTL_GPIO_COMMIT_FUNCTION_CONFIG_PINS failed. "
                        "Err=%Xh, devicePath = %wZ)\r\n",
                        status,
                        &devicePath);

            goto End;
        }
    }

    status = STATUS_SUCCESS;

End:

    // Cleanup on failure

    if (!NT_SUCCESS(status)) {
        if (extension->FunctionConfigHandle != NULL) {
            WdfObjectDelete(extension->FunctionConfigHandle);
            extension->FunctionConfigHandle = NULL;
        }
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, 
                "--SerialReserveFunctionConfig()\r\n");

    return status;
}

/*++

   EvtFileClose is called when all the handles represented by the FileObject
   is closed and all the references to FileObject is removed. This callback
   may get called in an arbitrary thread context instead of the thread that
   called CloseHandle. If you want to delete any per FileObject context that
   must be done in the context of the user thread that made the Create call,
   you should do that in the EvtDeviceCleanp callback.

Arguments:

    FileObject - Pointer to fileobject that represents the open handle.

Return Value:

   VOID

--*/
_Use_decl_annotations_
VOID
SerialEvtFileClose(
    WDFFILEOBJECT FileObject
    )
{
    WDFDEVICE device;
    PSERIAL_DEVICE_EXTENSION extension;

    PAGED_CODE();
    
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "++SerialEvtFileClose()\r\n");

    device=WdfFileObjectGetDevice(FileObject);
    extension=SerialGetDeviceExtension(device);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INTERRUPT, "SerialEvtFileClose() -"
            " disable all intrpts\r\n");

    // need to disable miniUart both interrupts.
    // Tx interrupt may have been disabled in ISR earlier.

    WRITE_INTERRUPT_ENABLE(extension, extension->Controller, 0x0); 

    SerialFileCloseWorker(device);

    // Release the function configuration while the UART is not in use.

    if (extension->FunctionConfigHandle != NULL) {
        WdfObjectDelete(extension->FunctionConfigHandle);
        extension->FunctionConfigHandle = NULL;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "--SerialEvtFileClose()\r\n");
    return;
}

/*++

Routine Description:

    This is the dispatch routine for IRP_MJ_CLOSE. This is called when all the
    handles represented by the FileObject is closed and all the references to
    the FileObject is removed.

Arguments:

    DeviceObject - Pointer to the device object for this device
    Irp - Pointer to the IRP for the current request

Return Value:

   NT status code

--*/
_Use_decl_annotations_
NTSTATUS
SerialWdmFileClose (
    WDFDEVICE Device,
    PIRP Irp
    )
{
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "++SerialWdmFileClose()\r\n");
    
    SerialFileCloseWorker(Device);

    Irp->IoStatus.Information = 0L;
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "--SerialWdmFileClose()\r\n");

    return STATUS_SUCCESS;
}

/*++

Routine Description:

    This routine is called to perform work when file handle is closed. 

Arguments:

    DeviceObject - Pointer to the device object for this device

Return Value:

   none

--*/
_Use_decl_annotations_
VOID
SerialFileCloseWorker(
    WDFDEVICE Device
    )
{
    ULONG flushCount;

    //
    // This "timer value" is used to wait 10 character times
    // after the hardware is empty before we actually "run down"
    // all of the flow control/break junk.
    //
    LARGE_INTEGER tenCharDelay;

    //
    // Holds a character time.
    //
    LARGE_INTEGER charTime;

    PSERIAL_DEVICE_EXTENSION extension = SerialGetDeviceExtension(Device);
    PSERIAL_INTERRUPT_CONTEXT interruptContext = SerialGetInterruptContext(extension->WdfInterrupt);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                "++SerialFileCloseWorker(%wZ)\r\n",
                &extension->DeviceName);

    //
    // Acquire the interrupt state lock.
    //
    WdfWaitLockAcquire(interruptContext->InterruptStateLock, NULL);

    //
    // If the Interrupts are connected, then the hardware state has to be
    // cleaned up now. Note that the EvtFileClose callback gets called for
    // an open file object even though the interrupts have been disabled
    // possibly  due to a Surprise Remove PNP event. In such a case, the
    // Interrupt  object should not be used.
    //
    if (interruptContext->IsInterruptConnected) 
    {

        charTime.QuadPart = -SerialGetCharTime(extension).QuadPart;
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                    "SerialFileCloseWorker() interrupt is connected. CharTime=%I64d (%08X%08Xh)\r\n",
                    charTime.QuadPart,charTime.HighPart,charTime.LowPart);

        // Do this now so that if the isr gets called it won't do anything
        // to cause more chars to get sent.  We want to run down the hardware.

        SetDeviceIsOpened(extension, FALSE, FALSE);

        // Synchronize with the isr to turn off break if it
        // is already on.

        WdfInterruptSynchronize(extension->WdfInterrupt,
                                SerialTurnOffBreak,
                                extension);

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                    "SerialFileCloseWorker() Wait until all characters emptied out of the hardware\r\n");
       
        // Wait a reasonable amount of time (20 * fifodepth) until all characters
        // have been emptied out of the hardware.

        for (flushCount = (20 * SERIAL_RX_FIFO_DEFAULT); flushCount != 0; flushCount--) 
        {
           if ((READ_LINE_STATUS(extension, extension->Controller) &
                (SERIAL_LSR_THRE | SERIAL_LSR_TEMT)) !=
               (SERIAL_LSR_THRE | SERIAL_LSR_TEMT)) {

              KeDelayExecutionThread(KernelMode, FALSE, &charTime);

          }  else {

             TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                        "SerialFileCloseWorker() emptied.\r\n");
             break;
          }
        }

        if (flushCount == 0) {  

            TraceEvents(TRACE_LEVEL_WARNING, DBG_CREATE_CLOSE,
                        "SerialFileCloseWorker() Failed to empty hardware.\r\n");

            SerialMarkHardwareBroken(extension);
        }

        // Synchronize with the ISR to let it know that interrupts are
        // no longer important.

        WdfInterruptSynchronize(extension->WdfInterrupt,
                                SerialMarkClose,
                                extension);

        // If the driver has automatically transmitted an Xoff in
        // the context of automatic receive flow control then we
        // should transmit an Xon.

        if (extension->RXHolding & SERIAL_RX_XOFF) 
        {

            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                        "SerialFileCloseWorker() driver has automatically transmitted an Xoff\r\n");

            // Loop until the holding register is empty.

            while (!(READ_LINE_STATUS(extension, extension->Controller) &
                     SERIAL_LSR_THRE)) {

                KeDelayExecutionThread(KernelMode,
                                        FALSE,
                                        &charTime);
            }

            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                        "SerialFileCloseWorker() now transmit an Xon\r\n");

            WRITE_TRANSMIT_HOLDING(extension,
                extension->Controller,
                extension->SpecialChars.XonChar);

            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                        "SerialFileCloseWorker() wait for char emptied from hardware\r\n");

            // Wait a reasonable amount of time for the characters
            // to be emptied out of the hardware.

             for (flushCount = (20 * 8); flushCount != 0; flushCount--) 
             {
                if ((READ_LINE_STATUS(extension, extension->Controller) &
                     (SERIAL_LSR_THRE | SERIAL_LSR_TEMT)) !=
                    (SERIAL_LSR_THRE | SERIAL_LSR_TEMT)) {
                   KeDelayExecutionThread(KernelMode, FALSE, &charTime);

                } else {

                    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                                "SerialFileCloseWorker() chars emptied\r\n");
                   break;
                }
             }

             if (flushCount == 0) {

                TraceEvents(TRACE_LEVEL_WARNING, DBG_CREATE_CLOSE,
                            "SerialFileCloseWorker() Failed to empty hardware.\r\n");
                SerialMarkHardwareBroken(extension);
             }
        } 


        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                        "SerialFileCloseWorker() Delay for 10 chars\r\n");

        // The hardware is empty.  Delay 10 character times before
        // shut down all the flow control.

        tenCharDelay.QuadPart = charTime.QuadPart * 10;

        KeDelayExecutionThread(KernelMode,
                                TRUE,
                                &tenCharDelay);

#pragma prefast(suppress: __WARNING_INFERRED_IRQ_TOO_LOW, "This warning is because we are calling interrupt synchronize routine directly.")

        SerialClrDTR(extension->WdfInterrupt, extension);

        // We have to be very careful how we clear the RTS line.
        // Transmit toggling might have been on at some point.
        //
        // We know that there is nothing left that could start
        // out the "polling"  execution path.  We need to
        // check the counter that indicates that the execution
        // path is active.  If it is then we loop delaying one
        // character time.  After each delay we check to see if
        // the counter has gone to zero.  When it has we know that
        // the execution path should be just about finished.  We
        // make sure that we still aren't in the routine that
        // synchronized execution with the ISR by synchronizing
        // ourselve with the ISR.

        if (extension->CountOfTryingToLowerRTS) {

            do {

#pragma prefast(suppress: __WARNING_INFERRED_IRQ_TOO_HIGH, "This warning is due to suppressing the previous one.")

                KeDelayExecutionThread(KernelMode,
                                        FALSE,
                                        &charTime);

            } while (extension->CountOfTryingToLowerRTS);

            // The execution path should no longer exist that
            // is trying to push down the RTS.  Well just
            // make sure it's down by falling through to
            // code that forces it down.

        }

#pragma prefast(suppress: __WARNING_INFERRED_IRQ_TOO_LOW, "This warning is because we are calling interrupt synchronize routine directly.")

        SerialClrRTS(extension->WdfInterrupt, extension);

         TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                        "SerialFileCloseWorker() Clean out the holding reasons\r\n");

        // Clean out the holding reasons (since we are closed).

        extension->RXHolding = 0;
        extension->TXHolding = 0;

        // Mark device as not busy for WMI

        extension->WmiCommData.IsBusy = FALSE;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                "SerialFileCloseWorker() Release the Interrupt state lock\r\n");

    // Release the Interrupt state lock.

    WdfWaitLockRelease(interruptContext->InterruptStateLock);

    // All is done.  The port has been disabled from interrupting
    // so there is no point in keeping the memory around.

    extension->BufferSize = 0;
    if (extension->InterruptReadBuffer != NULL) {
       ExFreePool(extension->InterruptReadBuffer);
    }
    extension->InterruptReadBuffer = NULL;

    // Make sure the wake is disabled.

    ASSERT(!extension->IsWakeEnabled);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                "SerialFileCloseWorker() draining DPCs and Timers\r\n");

    SerialDrainTimersAndDpcs(extension);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "DPC's drained:\r\n");

    // It's fine for the device to be powered off if there are no open handles.

    WdfDeviceResumeIdle(Device);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "SerialFileCloseWorker()"
            " ok for device to be removed.\r\n");

    // It's okay to allow the device to be stopped or removed.
    //
    // Note to anyone copying this sample as a starting point:
    //
    // This works in this driver simply because this driver supports exactly
    // one open handle at a time.  If it supported more, then it would need
    // counting logic to determine when all the reasons for failing Stop/Remove
    // were gone.

    WdfDeviceSetStaticStopRemove(Device, TRUE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                "--SerialFileCloseWorker(%wZ)\r\n",
                &extension->DeviceName);
    return;
}

/*++

Routine Description:

    This routine merely sets a boolean to true to mark the fact that
    somebody opened the device and its worthwhile to pay attention
    to interrupts.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialMarkOpen(
    WDFINTERRUPT  Interrupt,
    PVOID Context
    )
{

    PSERIAL_DEVICE_EXTENSION extension = Context;

    UNREFERENCED_PARAMETER(Interrupt);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "++SerialMarkOpen()\r\n");

    SerialReset(extension->WdfInterrupt, extension);

    // Prepare for the opening by re-enabling Rx interrupt.
    //
    // On PC we do this with 16550 uart by modifying the OUT2 line in the modem control,
    // since on 16550 this bit is "anded" with the interrupt line.
    // on RPi there is no OUT2 so we enable Rx interrupt now

    WRITE_MODEM_CONTROL(extension,
        extension->Controller,
        (UCHAR)(READ_MODEM_CONTROL(extension, extension->Controller) | SERIAL_MCR_OUT2));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INTERRUPT, "SerialMarkOpen() - enable Rx interrupt\r\n");

    WRITE_INTERRUPT_ENABLE(extension, extension->Controller, SERIAL_IER_RDA); 

    extension->DeviceIsOpened = TRUE;
    extension->ErrorWord = 0;

#if DBG
    PrintMiniUartregs(extension);
#endif

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "--SerialMarkOpen()\r\n");
    return FALSE;

}

/*++

Routine Description:

    This routine waits until all characters ot become emptied out of the hardware.

Arguments:

    PDevExt - a pointer to the device extension.
    PDrainTime - time duration to wait for

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialDrainUART(PSERIAL_DEVICE_EXTENSION DevExt,
                PLARGE_INTEGER PDrainTime)
{
   PAGED_CODE();
   TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "++SerialDrainUART()\r\n");

   // Wait until all characters have been emptied out of the hardware.

   while ((READ_LINE_STATUS(DevExt, DevExt->Controller) &
           (SERIAL_LSR_THRE | SERIAL_LSR_TEMT))
           != (SERIAL_LSR_THRE | SERIAL_LSR_TEMT)) {

        KeDelayExecutionThread(KernelMode, FALSE, PDrainTime);
    }
   TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "--SerialDrainUART()\r\n");
}

/*++

Routine Description:

    This routine disables the UART and puts it in a "safe" state when
    not in use (like a close or powerdown).

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
VOID
SerialDisableUART(PVOID Context)
{
    PSERIAL_DEVICE_EXTENSION extension = Context;
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "++SerialDisableUART()\r\n");

    // Prepare for the closing by stopping interrupts.

    // On PC we do this with 16550 uart by modifying the OUT2 line in the modem control,
    // since on 16550 this bit is "anded" with the interrupt line.

   WRITE_MODEM_CONTROL(extension, extension->Controller,
                        (UCHAR)(READ_MODEM_CONTROL(extension, extension->Controller)
                        & ~SERIAL_MCR_OUT2));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE,
                "SerialDisableUART() - disable all interrupts\r\n");

    // on RPi there is no OUT2 so we disable interrupts here

    WRITE_INTERRUPT_ENABLE(extension, extension->Controller, 0x0); 

    if (extension->FifoPresent) {
        WRITE_FIFO_CONTROL(extension, extension->Controller, (UCHAR)0);
    }

   TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "--SerialDisableUART()\r\n");
   return;
}


/*++

Routine Description:

    This routine merely sets a boolean to false to mark the fact that
    somebody closed the device and it's no longer worthwhile to pay attention
    to interrupts.  It also disables the UART.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialMarkClose(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{

    PSERIAL_DEVICE_EXTENSION extension = Context;

    UNREFERENCED_PARAMETER(Interrupt);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "++SerialMarkClose()\r\n");

    SerialDisableUART(Context);

    extension->DeviceIsOpened = FALSE;
    extension->DeviceState.Reopen   = FALSE;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "--SerialMarkClose()\r\n");
    return FALSE;
}

/*++

Routine Description:

    This function will return the number of 100 nanosecond intervals
    there are in one character time (based on the present form
    of flow control.

Arguments:

    Extension - Just what it says.

Return Value:

    100 nanosecond intervals in a character time.

--*/
_Use_decl_annotations_
LARGE_INTEGER
SerialGetCharTime(
    PSERIAL_DEVICE_EXTENSION Extension
    )
{
    ULONG dataSize = 0;
    ULONG paritySize;
    ULONG stopSize;
    ULONG charTime;
    ULONG bitTime;
    LARGE_INTEGER tmp;

    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "++SerialGetCharTime()\r\n");

    if ((Extension->LineControl & SERIAL_DATA_MASK)
                == SERIAL_7_DATA) {
        dataSize = 7;
    } else if ((Extension->LineControl & SERIAL_DATA_MASK)
                == SERIAL_8_DATA) {
        dataSize = 8;
    }

    paritySize = 1;
    if ((Extension->LineControl & SERIAL_PARITY_MASK)
            == SERIAL_NONE_PARITY) {

        paritySize = 0;
    }

     stopSize = 1;

    // First we calculate the number of 100 nanosecond intervals
    // are in a single bit time (approximately).

    bitTime = (10000000+(Extension->CurrentBaud-1))/Extension->CurrentBaud;
    charTime = bitTime + ((dataSize+paritySize+stopSize)*bitTime);

    tmp.QuadPart = charTime;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "--SerialGetCharTime()"
        " chartime=%I64d (%08X%08Xh)\r\n", tmp.QuadPart, tmp.HighPart, tmp.LowPart);
    return tmp;
}

