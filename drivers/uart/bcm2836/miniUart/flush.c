// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//
//    flush.c
//
// Abstract:
//
//    This module contains the code that is very specific to flush
//    operations in the serial driver


#include "precomp.h"
#include "flush.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SerialFlush)
#endif

/*++

Routine Description:

    This is the dispatch routine for flush.  Flushing works by placing
    this request in the write queue.  When this request reaches the
    front of the write queue we simply complete it since this implies
    that all previous writes have completed.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    Could return status success, cancelled, or pending.

--*/
_Use_decl_annotations_
NTSTATUS
SerialFlush(
    WDFDEVICE Device,
    PIRP Irp
    )
{
    PSERIAL_DEVICE_EXTENSION extension;

    extension = SerialGetDeviceExtension(Device);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "++SerialFlush(%p, %p)\r\n",
                Device,
                Irp);

    PAGED_CODE();

    WdfIoQueueStopSynchronously(extension->WriteQueue);
    
    // Flush is done - restart the queue
    
    WdfIoQueueStart(extension->WriteQueue);

    Irp->IoStatus.Information = 0L;
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "--SerialFlush\r\n");

    return STATUS_SUCCESS;
 }

