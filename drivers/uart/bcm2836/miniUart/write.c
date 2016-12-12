// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    write.c
//
// Abstract:
//
//   This module contains the code that is very specific to write
//   operations in RPi mini Uart serial driver

#include "precomp.h"
#include "write.tmh"

EVT_WDF_REQUEST_CANCEL SerialCancelCurrentWrite;
EVT_WDF_REQUEST_CANCEL SerialCancelCurrentXoff;
EVT_WDF_INTERRUPT_SYNCHRONIZE SerialGiveWriteToIsr;
EVT_WDF_INTERRUPT_SYNCHRONIZE SerialGiveXoffToIsr;
EVT_WDF_INTERRUPT_SYNCHRONIZE SerialGrabWriteFromIsr;
EVT_WDF_INTERRUPT_SYNCHRONIZE SerialGrabXoffFromIsr;

/*++

Routine Description:

    This is the dispatch routine for write.  It validates the parameters
    for the write request and if all is ok then it places the request
    on the work queue.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Pointer to the WDFREQUEST for the current request

    Length - Length of the IO operation
                 The default property of the queue is to not dispatch
                 zero lenght read & write requests to the driver and
                 complete is with status success. So we will never get
                 a zero length request.

Return Value:

--*/
_Use_decl_annotations_
VOID
SerialEvtIoWrite(
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t Length
    )
{
    PSERIAL_DEVICE_EXTENSION extension;
    NTSTATUS status;
    WDFDEVICE hDevice;
    WDF_REQUEST_PARAMETERS params;
    PREQUEST_CONTEXT reqContext;
    size_t bufLen;
    UCHAR tempIER=0x00;

    hDevice = WdfIoQueueGetDevice(Queue);
    extension = SerialGetDeviceExtension(hDevice);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "++SerialEvtIoWrite(%p, 0x%I64x)\r\n", 
                Request,
                Length);

    if (SerialCompleteIfError(extension, Request) != STATUS_SUCCESS) {

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                    "--SerialEvtIoWrite 1 %Xh\r\n",
                    (ULONG)STATUS_CANCELLED);
        return;
    }

    WDF_REQUEST_PARAMETERS_INIT(&params);

    WdfRequestGetParameters(Request, &params);

    // Initialize the scratch area of the request.

    reqContext = SerialGetRequestContext(Request);
    reqContext->MajorFunction = params.Type;
    reqContext->Length = (ULONG) Length;

    status = WdfRequestRetrieveInputBuffer (Request,
                                            Length,
                                            &reqContext->SystemBuffer,
                                            &bufLen);

    if (!NT_SUCCESS (status)) {

        SerialCompleteRequest(Request , status, 0);
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                    "--SerialEvtIoWrite 2 %Xh\r\n", 
                    status);
        return;
    }

   SerialStartOrQueue(extension,
                        Request,
                        extension->WriteQueue,
                        &extension->CurrentWriteRequest,
                        SerialStartWrite);

    // enable mini Uart Tx interrupt

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INTERRUPT, "SerialEvtIoWrite() - enable Tx interrupt\r\n");

    tempIER=READ_INTERRUPT_ENABLE(extension, extension->Controller);
    WRITE_INTERRUPT_ENABLE(extension, extension->Controller, (tempIER | SERIAL_IER_THR));

   TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "--SerialEvtIoWrite()=%X\r\n", status);
   return;
}

/*++

Routine Description:

    This routine is used to start off any write.  It initializes
    the Iostatus fields of the request.  It will set up any timers
    that are used to control the write.

Arguments:

    Extension - Points to the serial device extension

Return Value:

--*/
_Use_decl_annotations_
VOID
SerialStartWrite(
    PSERIAL_DEVICE_EXTENSION Extension
    )
{

    LARGE_INTEGER    totalTime;
    BOOLEAN          useAtimer;
    SERIAL_TIMEOUTS  timeouts;
    PREQUEST_CONTEXT reqContext;
    PREQUEST_CONTEXT reqContextXoff;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                     "++SerialStartWrite(%p)\r\n", Extension);

    totalTime.QuadPart = 0;

    do {

        reqContext = SerialGetRequestContext(Extension->CurrentWriteRequest);

        // If there is an xoff counter then complete it.

        // We see if there is a actually an Xoff counter request.
        //
        // If there is, we put the write request back on the head
        // of the write list.  We then complete the xoff counter.
        // The xoff counter completing code will actually make the
        // xoff counter back into the current write request, and
        // in the course of completing the xoff (which is now
        // the current write) we will restart this request.

        if (Extension->CurrentXoffRequest) {

            reqContextXoff =
                SerialGetRequestContext(Extension->CurrentXoffRequest);

            if (SERIAL_REFERENCE_COUNT(reqContextXoff)) {

                // The reference count is non-zero.  This implies that
                // the xoff request has not made it through the completion
                // path yet.  We will increment the reference count
                // and attempt to complete it ourseleves.

                SERIAL_SET_REFERENCE(reqContextXoff, SERIAL_REF_XOFF_REF);

                reqContextXoff->Information = 0;

                // The following call will actually release the
                // cancel spin lock.

                SerialTryToCompleteCurrent(Extension,
                                            SerialGrabXoffFromIsr,
                                            STATUS_SERIAL_MORE_WRITES,
                                            &Extension->CurrentXoffRequest,
                                            NULL,
                                            NULL,
                                            Extension->XoffCountTimer,
                                            NULL,
                                            NULL,
                                            SERIAL_REF_XOFF_REF);

            } else {

                // The request is well on its way to being finished.
                // We can let the regular completion code do the
                // work.  Just release the spin lock.

            }

        }

        useAtimer = FALSE;

        // Calculate the timeout value needed for the
        // request.  Note that the values stored in the
        // timeout record are in milliseconds.  Note that
        // if the timeout values are zero then we won't start
        // the timer.

        timeouts = Extension->timeouts;

        if (timeouts.WriteTotalTimeoutConstant ||
            timeouts.WriteTotalTimeoutMultiplier) {

            useAtimer = TRUE;

            // We have some timer values to calculate.
            //
            // Take care, we might have an xoff counter masquerading
            // as a write.

            totalTime.QuadPart =((LONGLONG)((UInt32x32To64((reqContext->MajorFunction == IRP_MJ_WRITE)?
                                                    (reqContext->Length) : (1),
                                                    timeouts.WriteTotalTimeoutMultiplier)
                                                    + timeouts.WriteTotalTimeoutConstant)))
                                                    * -10000;
        }

        // The request may be going to the isr shortly.  Now
        // is a good time to initialize its reference counts.

        SERIAL_INIT_REFERENCE(reqContext);

         // We give the request to to the isr to write out.
         // We set a cancel routine that knows how to
         // grab the current write away from the isr.

         SerialSetCancelRoutine(Extension->CurrentWriteRequest,
                                 SerialCancelCurrentWrite);

        if (useAtimer) {
            BOOLEAN result;

            result = SerialSetTimer(Extension->WriteRequestTotalTimer,
                                    totalTime);

            if(result == FALSE) {
                
                // This timer now has a reference to the request.

                SERIAL_SET_REFERENCE(reqContext, SERIAL_REF_TOTAL_TIMER );
            }
        }

        WdfInterruptSynchronize(Extension->WdfInterrupt,
                                SerialGiveWriteToIsr,
                                Extension);

    } while (FALSE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "--SerialStartWrite\r\n");
    return;
}

/*++

Routine Description:

    This routine completes the old write as well as getting
    a pointer to the next write.

    The reason that we have have pointers to the current write
    queue as well as the current write request is so that this
    routine may be used in the common completion code for
    read and write.

Arguments:

    CurrentOpRequest - Pointer to the pointer that points to the
                   current write request.

    QueueToProcess - Pointer to the write queue.

    NewRequest - A pointer to a pointer to the request that will be the
             current request.  Note that this could end up pointing
             to a null pointer.  This does NOT necessaryly mean
             that there is no current write.  What could occur
             is that while the cancel lock is held the write
             queue ended up being empty, but as soon as we release
             the cancel spin lock a new request came in from
             SerialStartWrite.

    CompleteCurrent - Flag indicates whether the CurrentOpRequest should
                      be completed.

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialGetNextWrite(
    WDFREQUEST* CurrentOpRequest,
    WDFQUEUE QueueToProcess,
    WDFREQUEST* NewRequest,
    BOOLEAN CompleteCurrent,
    PSERIAL_DEVICE_EXTENSION Extension
    )
{
    PREQUEST_CONTEXT reqContext;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "++SerialGetNextWrite\r\n");

    do {

        reqContext = SerialGetRequestContext(*CurrentOpRequest);

        // We could be completing a flush.

        if (reqContext->MajorFunction == IRP_MJ_WRITE) {

            ASSERT(Extension->TotalCharsQueued >= reqContext->Length);

            Extension->TotalCharsQueued -= reqContext->Length;

        } else if (reqContext->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

            WDFREQUEST request = *CurrentOpRequest;
            PSERIAL_XOFF_COUNTER xc;

            xc = reqContext->SystemBuffer;

            // We should never have a xoff counter when we
            // get to this point.

            ASSERT(!Extension->CurrentXoffRequest);

            // This could only be a xoff counter masquerading as
            // a write request.

            Extension->TotalCharsQueued--;

            // Check to see of the xoff request has been set with success.
            // This means that the write completed normally.  If that
            // is the case, and it hasn't been set to cancel in the
            // meanwhile, then go on and make it the CurrentXoffRequest.

            if (reqContext->Status != STATUS_SUCCESS || reqContext->Cancelled) {

                // If Xoff request getting abandoned due to loss of
                // Total timer - SERIAL_REF_TOTAL_TIMER
                // we can just finish it off.

            } else {

                SerialSetCancelRoutine(request, SerialCancelCurrentXoff);

                // We don't want to complete the current request now.  This
                // will now get completed by the Xoff counter code.

                CompleteCurrent = FALSE;

                // Give the counter to the isr.

                Extension->CurrentXoffRequest = request;

                WdfInterruptSynchronize(Extension->WdfInterrupt,
                                        SerialGiveXoffToIsr,
                                        Extension);

                // Start the timer for the counter and increment
                // the reference count since the timer has a
                // reference to the request.

                if (xc->Timeout) {

                    LARGE_INTEGER delta;
                    BOOLEAN result;

                    delta.QuadPart = -((LONGLONG)UInt32x32To64(1000,
                                                                xc->Timeout));

                    result = SerialSetTimer(Extension->XoffCountTimer,
                                            delta);

                    if(result == FALSE) {

                        SERIAL_SET_REFERENCE(reqContext,
                                            SERIAL_REF_TOTAL_TIMER);

                    }
                }
            }
        }

        // Note that the following call will (probably) also cause
        // the current request to be completed.

        SerialGetNextRequest(CurrentOpRequest,
                            QueueToProcess,
                            NewRequest,
                            CompleteCurrent,
                            Extension);

        if (!*NewRequest) {

            WdfInterruptSynchronize(Extension->WdfInterrupt,
                                    SerialProcessEmptyTransmit,
                                    Extension);

            break;

        } else if (SerialGetRequestContext(*NewRequest)->MajorFunction
                   == IRP_MJ_FLUSH_BUFFERS) {

            // If we encounter a flush request we just want to get
            // the next request and complete the flush.
            //
            // Note that if NewRequest is non-null then it is also
            // equal to CurrentWriteRequest.

            ASSERT((*NewRequest) == (*CurrentOpRequest));
            SerialGetRequestContext(*NewRequest)->Status = STATUS_SUCCESS;

        } else {

            break;
        }

    } while (TRUE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "--SerialGetNextWrite\r\n");
}

/*++

Routine Description:

    This routine is merely used to complete any write.  It
    assumes that the status and the information fields of
    the request are already correctly filled in.

Arguments:

    Dpc - Not Used.

    DeferredContext - Really points to the device extension.

    SystemContext1 - Not Used.

    SystemContext2 - Not Used.

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialCompleteWrite(
    WDFDPC Dpc
    )
{
    PSERIAL_DEVICE_EXTENSION Extension = NULL;

    Extension = SerialGetDeviceExtension(WdfDpcGetParentObject(Dpc));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "++SerialCompleteWrite(%p) DPC\r\n",
                     Extension);

    SerialTryToCompleteCurrent(Extension,
                               NULL,
                               STATUS_SUCCESS,
                               &Extension->CurrentWriteRequest,
                               Extension->WriteQueue,
                               NULL,
                               Extension->WriteRequestTotalTimer,
                               SerialStartWrite,
                               SerialGetNextWrite,
                               SERIAL_REF_ISR);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "--SerialCompleteWrite DPC\r\n");
}

/*++

Routine Description:

    This routine is used to determine if conditions are appropriate
    to satisfy a wait for transmit empty event, and if so to complete
    the request that is waiting for that event.  It also call the code
    that checks to see if we should lower the RTS line if we are
    doing transmit toggling.

    NOTE: This routine is called by WdfInterruptSynchronize.

    NOTE: This routine assumes that it is called with the cancel
          spinlock held.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialProcessEmptyTransmit(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = Context;

    UNREFERENCED_PARAMETER(Interrupt);

    if (extension->IsrWaitMask && (extension->IsrWaitMask & SERIAL_EV_TXEMPTY) &&
        extension->EmptiedTransmit && (!extension->TransmitImmediate) &&
        (!extension->CurrentWriteRequest) && IsQueueEmpty(extension->WriteQueue)) {

        extension->HistoryMask |= SERIAL_EV_TXEMPTY;
        if (extension->IrpMaskLocation) {

            *extension->IrpMaskLocation = extension->HistoryMask;
            extension->IrpMaskLocation = NULL;
            extension->HistoryMask = 0;

            SerialGetRequestContext(extension->CurrentWaitRequest)->Information = sizeof(ULONG);

            SerialInsertQueueDpc(extension->CommWaitDpc);
        }

        extension->CountOfTryingToLowerRTS++;
        SerialPerhapsLowerRTS(extension->WdfInterrupt, extension);
    }

    return FALSE;
}

/*++

Routine Description:

    Try to start off the write by slipping it in behind
    a transmit immediate char, or if that isn't available
    and the transmit holding register is empty, "tickle"
    the UART into interrupting with a transmit buffer
    empty.

    NOTE: This routine is called by WdfInterruptSynchronize.

    NOTE: This routine assumes that it is called with the
          cancel spin lock held.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialGiveWriteToIsr(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = Context;

    // The current stack location.  This contains all of the
    // information we need to process this particular request.

    PREQUEST_CONTEXT reqContext;

    UNREFERENCED_PARAMETER(Interrupt);

    reqContext = SerialGetRequestContext(extension->CurrentWriteRequest);

    // We might have a xoff counter request masquerading as a
    // write.  The length of these requests will always be one
    // and we can get a pointer to the actual character from
    // the data supplied by the user.

    if (reqContext->MajorFunction == IRP_MJ_WRITE) {

        extension->WriteLength = reqContext->Length;
        extension->WriteCurrentChar = reqContext->SystemBuffer;

    } else {

        extension->WriteLength = 1;
        extension->WriteCurrentChar =
            ((PUCHAR)reqContext->SystemBuffer) +
            FIELD_OFFSET(SERIAL_XOFF_COUNTER, XoffChar);
    }

    // The isr now has a reference to the request.

    SERIAL_SET_REFERENCE(reqContext, SERIAL_REF_ISR);

    // Check first to see if an immediate char is transmitting.
    // If it is then we'll just slip in behind it when its done.

    if (!extension->TransmitImmediate) {

        // If there is no immediate char transmitting then we
        // will "re-enable" the transmit holding register empty
        // interrupt.  The 16550 family of devices will always
        // signal a transmit holding register empty interrupt
        // *ANY* time this bit is set to one.  By doing things
        // this way we can simply use the normal interrupt code
        // to start off this write.
        //
        // We've been keeping track of whether the transmit holding
        // register is empty so it we only need to do this
        // if the register is empty.

        if (extension->HoldingEmpty) {

            DISABLE_ALL_INTERRUPTS(extension, extension->Controller);
            ENABLE_ALL_INTERRUPTS(extension, extension->Controller);
        }
    }

    // The rts line may already be up from previous writes,
    // however, it won't take much additional time to turn
    // on the RTS line if we are doing transmit toggling.

    if ((extension->HandFlow.FlowReplace & SERIAL_RTS_MASK) ==
        SERIAL_TRANSMIT_TOGGLE) {

        SerialSetRTS(extension->WdfInterrupt, extension);
    }

    return FALSE;
}

/*++

Routine Description:

    This routine is used to cancel the current write.

Arguments:

    Device - Wdf handle for the device

    Request - Pointer to the WDFREQUEST to be canceled.

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialCancelCurrentWrite(
    WDFREQUEST Request
    )
{
    PSERIAL_DEVICE_EXTENSION extension;
    WDFDEVICE device = WdfIoQueueGetDevice(WdfRequestGetIoQueue(Request));

    UNREFERENCED_PARAMETER(Request);

    extension = SerialGetDeviceExtension(device);

    SerialTryToCompleteCurrent(extension,
                                SerialGrabWriteFromIsr,
                                STATUS_CANCELLED,
                                &extension->CurrentWriteRequest,
                                extension->WriteQueue,
                                NULL,
                                extension->WriteRequestTotalTimer,
                                SerialStartWrite,
                                SerialGetNextWrite,
                                SERIAL_REF_CANCEL);
}

/*++

Routine Description:

    This routine will try to timeout the current write.

Arguments:

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialWriteTimeout(
    WDFTIMER Timer
    )
{
    PSERIAL_DEVICE_EXTENSION extension = NULL;

    extension = SerialGetDeviceExtension(WdfTimerGetParentObject(Timer));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "++SerialWriteTimeout(%p)\r\n",
                     extension);

    SerialTryToCompleteCurrent(extension,
                                SerialGrabWriteFromIsr,
                                STATUS_TIMEOUT,
                                &extension->CurrentWriteRequest,
                                extension->WriteQueue,
                                NULL,
                                extension->WriteRequestTotalTimer,
                                SerialStartWrite,
                                SerialGetNextWrite,
                                SERIAL_REF_TOTAL_TIMER);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "--SerialWriteTimeout\r\n");
}

/*++

Routine Description:

    This routine is used to grab the current request, which could be timing
    out or canceling, from the ISR

    NOTE: This routine is being called from WdfInterruptSynchronize.

    NOTE: This routine assumes that the cancel spin lock is held
          when this routine is called.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    Always false.

--*/
_Use_decl_annotations_
BOOLEAN
SerialGrabWriteFromIsr(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{

    PSERIAL_DEVICE_EXTENSION extension = Context;

    PREQUEST_CONTEXT reqContext;

    UNREFERENCED_PARAMETER(Interrupt);

    reqContext = SerialGetRequestContext(extension->CurrentWriteRequest);

    // Check if the write length is non-zero.  If it is non-zero
    // then the ISR still owns the request. We calculate the the number
    // of characters written and update the information field of the
    // request with the characters written.  We then clear the write length
    // the isr sees.

    if (extension->WriteLength) {

        // We could have an xoff counter masquerading as a
        // write request.  If so, don't update the write length.

        if (reqContext->MajorFunction == IRP_MJ_WRITE) {

            reqContext->Information = reqContext->Length -extension->WriteLength;

        } else {

            reqContext->Information = 0;
        }

        // Since the isr no longer references this request, we can
        // decrement it's reference count.

        SERIAL_CLEAR_REFERENCE(reqContext, SERIAL_REF_ISR);

        extension->WriteLength = 0;
    }

    return FALSE;

}

/*++

Routine Description:

    This routine is used to grab an xoff counter request from the
    isr when it is no longer masquerading as a write request.  This
    routine is called by the cancel and timeout code for the
    xoff counter ioctl.


    NOTE: This routine is being called from WdfInterruptSynchronize.

    NOTE: This routine assumes that the cancel spin lock is held
          when this routine is called.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    Always false.

--*/
_Use_decl_annotations_
BOOLEAN
SerialGrabXoffFromIsr(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{

    PSERIAL_DEVICE_EXTENSION extension = Context;

    PREQUEST_CONTEXT reqContext;

    UNREFERENCED_PARAMETER(Interrupt);

    reqContext = SerialGetRequestContext(extension->CurrentXoffRequest);

    if (extension->CountSinceXoff) {

        // This is only non-zero when there actually is a Xoff ioctl
        // counting down.

        extension->CountSinceXoff = 0;

        // We decrement the count since the isr no longer owns
        // the request.

        SERIAL_CLEAR_REFERENCE(reqContext, SERIAL_REF_ISR);
    }

    return FALSE;
}

/*++

Routine Description:

    This routine is merely used to truely complete an xoff counter request.  It
    assumes that the status and the information fields of the request are
    already correctly filled in.

Arguments:

    Dpc - Not Used.

    DeferredContext - Really points to the device extension.

    SystemContext1 - Not Used.

    SystemContext2 - Not Used.

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialCompleteXoff(
    WDFDPC Dpc
    )
{

    PSERIAL_DEVICE_EXTENSION extension = NULL;

    extension = SerialGetDeviceExtension(WdfDpcGetParentObject(Dpc));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "++SerialCompleteXoff(%p)\r\n",
                     extension);

    SerialTryToCompleteCurrent(extension,
                                NULL,
                                STATUS_SUCCESS,
                                &extension->CurrentXoffRequest,
                                NULL, NULL,
                                extension->XoffCountTimer,
                                NULL, NULL,
                                SERIAL_REF_ISR);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "--SerialCompleteXoff\r\n");
}

/*++

Routine Description:

    This routine is merely used to truely complete an xoff counter request,
    if its timer has run out.

Arguments:


Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialTimeoutXoff(
    WDFTIMER Timer
    )
{

    PSERIAL_DEVICE_EXTENSION extension = NULL;

    extension = SerialGetDeviceExtension(WdfTimerGetParentObject(Timer));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "++SerialTimeoutXoff(%p)\r\n", extension);

    SerialTryToCompleteCurrent(extension,
                                SerialGrabXoffFromIsr,
                                STATUS_SERIAL_COUNTER_TIMEOUT,
                                &extension->CurrentXoffRequest,
                                NULL, NULL, NULL,
                                NULL, NULL,
                                SERIAL_REF_TOTAL_TIMER);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "--SerialTimeoutXoff\r\n");
}

/*++

Routine Description:

    This routine is used to cancel the current write.

Arguments:

    Device - Wdf handle for the device

    Request - Pointer to the WDFREQUEST to be canceled.

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialCancelCurrentXoff(
    WDFREQUEST Request
    )
{
    PSERIAL_DEVICE_EXTENSION extension;
    WDFDEVICE device = WdfIoQueueGetDevice(WdfRequestGetIoQueue(Request));

    UNREFERENCED_PARAMETER(Request);

    extension = SerialGetDeviceExtension(device);

    SerialTryToCompleteCurrent(extension,
                                SerialGrabXoffFromIsr,
                                STATUS_CANCELLED,
                                &extension->CurrentXoffRequest,
                                NULL,
                                NULL,
                                extension->XoffCountTimer,
                                NULL,
                                NULL,
                                SERIAL_REF_CANCEL);
}

/*++

Routine Description:


    This routine starts off the xoff counter.  It merely
    has to set the xoff count and increment the reference
    count to denote that the isr has a reference to the request.

    NOTE: This routine is called by WdfInterruptSynchronize.

    NOTE: This routine assumes that it is called with the
          cancel spin lock held.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialGiveXoffToIsr(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = Context;
    PREQUEST_CONTEXT reqContext;
    PSERIAL_XOFF_COUNTER xc = NULL;

    UNREFERENCED_PARAMETER(Interrupt);

    reqContext = SerialGetRequestContext(extension->CurrentXoffRequest);
    xc = reqContext->SystemBuffer;

    // The current stack location.  This contains all of the
    // information we need to process this particular request.

    ASSERT(extension->CurrentXoffRequest);
    extension->CountSinceXoff = xc->Counter;

    // The isr now has a reference to the request.

    SERIAL_SET_REFERENCE(reqContext, SERIAL_REF_ISR);

    return FALSE;
}


