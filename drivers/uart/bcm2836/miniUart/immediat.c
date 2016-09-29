// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//   immediat.c
//
// Abstract:
//
//    This module contains the code that is very specific to transmit
//    immediate character operations in the serial driver

#include "precomp.h"
#include "immediat.tmh"

VOID
SerialGetNextImmediate(
    _In_ WDFREQUEST* CurrentOpRequest,
    _In_ WDFQUEUE QueueToProcess,
    _In_ WDFREQUEST* NewRequest,
    _In_ BOOLEAN CompleteCurrent,
    _In_ PSERIAL_DEVICE_EXTENSION Extension);

EVT_WDF_REQUEST_CANCEL SerialCancelImmediate;
EVT_WDF_INTERRUPT_SYNCHRONIZE SerialGiveImmediateToIsr;
EVT_WDF_INTERRUPT_SYNCHRONIZE SerialGrabImmediateFromIsr;

/*++

Routine Description:

    This routine will calculate the timeouts needed for the
    write.  It will then hand the request off to the isr.  It
    will need to be careful incase the request has been canceled.

Arguments:

    Extension - A pointer to the serial device extension.

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialStartImmediate(
    PSERIAL_DEVICE_EXTENSION Extension
    )
{
    LARGE_INTEGER totalTime = {0};
    BOOLEAN useAtimer;
    SERIAL_TIMEOUTS timeouts;
    PREQUEST_CONTEXT reqContext;

    reqContext = SerialGetRequestContext(Extension->CurrentImmediateRequest);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "++SerialStartImmediate(%p)\r\n",
                Extension);

    useAtimer = FALSE;
    reqContext->Status = STATUS_PENDING;

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

        totalTime.QuadPart
           = (LONGLONG)((ULONG)timeouts.WriteTotalTimeoutMultiplier);

        totalTime.QuadPart += timeouts.WriteTotalTimeoutConstant;

        totalTime.QuadPart *= -10000;
    }

    // As the request might be going to the isr, this is a good time
    // to initialize the reference count.

    SERIAL_INIT_REFERENCE(reqContext);

     // We give the request to to the isr to write out.
     // We set a cancel routine that knows how to
     // grab the current write away from the isr.

    SerialSetCancelRoutine(Extension->CurrentImmediateRequest,
                            SerialCancelImmediate);

    if (useAtimer) {
        BOOLEAN result;

        result = SerialSetTimer(Extension->ImmediateTotalTimer,
                                totalTime);

        if(result == FALSE) {

            // Since the timer knows about the request we increment
            // the reference count.

            SERIAL_SET_REFERENCE(reqContext,
                                SERIAL_REF_TOTAL_TIMER);
        }
    }

    WdfInterruptSynchronize(Extension->WdfInterrupt,
                            SerialGiveImmediateToIsr,
                            Extension);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
                "--SerialStartImmediate\r\n");
}

/*++

Routine Description:

    This routine completes immediate operation.

Arguments:

    Dpc - A pointer serial DPC.

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialCompleteImmediate(
    WDFDPC Dpc
    )
{
    PSERIAL_DEVICE_EXTENSION Extension = NULL;

    Extension = SerialGetDeviceExtension(WdfDpcGetParentObject(Dpc));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
                "++SerialCompleteImmediate(%p)\r\n",
                Extension);

    SerialTryToCompleteCurrent(Extension,
                                NULL,
                                STATUS_SUCCESS,
                                &Extension->CurrentImmediateRequest,
                                NULL,
                                NULL,
                                Extension->ImmediateTotalTimer,
                                NULL,
                                SerialGetNextImmediate,
                                SERIAL_REF_ISR);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "--SerialCompleteImmediate\r\n");
}

/*++

Routine Description:

    This routine times out immediate operation.

Arguments:

    Timer - A pointer to timer.

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialTimeoutImmediate(
    WDFTIMER Timer
    )
{
    PSERIAL_DEVICE_EXTENSION extension = NULL;

    extension = SerialGetDeviceExtension(WdfTimerGetParentObject(Timer));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
                "++SerialTimeoutImmediate(%p)\r\n",
                extension);

    SerialTryToCompleteCurrent(extension,
                               SerialGrabImmediateFromIsr,
                               STATUS_TIMEOUT,
                               &extension->CurrentImmediateRequest,
                               NULL,
                               NULL,
                               extension->ImmediateTotalTimer,
                               NULL,
                               SerialGetNextImmediate,
                               SERIAL_REF_TOTAL_TIMER);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "--SerialTimeoutImmediate\r\n");
}

/*++

Routine Description:

    This routine is used to complete the current immediate
    request.  Even though the current immediate will always
    be completed and there is no queue associated with it,
    we use this routine so that we can try to satisfy
    a wait for transmit queue empty event.

Arguments:

    CurrentOpRequest - Pointer to the pointer that points to the
                   current write request.  This should point
                   to CurrentImmediateRequest.

    QueueToProcess - Always NULL.

    NewRequest - Always NULL on exit to this routine.

    CompleteCurrent - Should always be true for this routine.


Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialGetNextImmediate(
    WDFREQUEST* CurrentOpRequest,
    WDFQUEUE QueueToProcess,
    WDFREQUEST* NewRequest,
    BOOLEAN CompleteCurrent,
    PSERIAL_DEVICE_EXTENSION Extension
    )
{
    WDFREQUEST oldRequest = *CurrentOpRequest;
    PREQUEST_CONTEXT reqContext = SerialGetRequestContext(oldRequest);

    UNREFERENCED_PARAMETER(QueueToProcess);
    UNREFERENCED_PARAMETER(CompleteCurrent);


    ASSERT(Extension->TotalCharsQueued >= 1);
    Extension->TotalCharsQueued--;

    *CurrentOpRequest = NULL;
    *NewRequest = NULL;

     WdfInterruptSynchronize(Extension->WdfInterrupt,
                             SerialProcessEmptyTransmit,
                             Extension);

    SerialCompleteRequest(oldRequest, reqContext->Status, reqContext->Information);
}

/*++

Routine Description:

    This routine is used to cancel a request that is waiting on
    a comm event.

Arguments:

    Request - Pointer to the WDFREQUEST for the current request

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialCancelImmediate(
    WDFREQUEST Request
    )
{
    PSERIAL_DEVICE_EXTENSION extension = NULL;
    WDFDEVICE device = WdfIoQueueGetDevice(WdfRequestGetIoQueue(Request));

    UNREFERENCED_PARAMETER(Request);

    extension = SerialGetDeviceExtension(device);

    SerialTryToCompleteCurrent(extension,
                               SerialGrabImmediateFromIsr,
                               STATUS_CANCELLED,
                               &extension->CurrentImmediateRequest,
                               NULL,
                               NULL,
                               extension->ImmediateTotalTimer,
                               NULL,
                               SerialGetNextImmediate,
                               SERIAL_REF_CANCEL);
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
SerialGiveImmediateToIsr(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = Context;
    PREQUEST_CONTEXT reqContext;

    UNREFERENCED_PARAMETER(Interrupt);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INTERRUPT, "++SerialGiveImmediateToIsr()\r\n");

    reqContext = SerialGetRequestContext(extension->CurrentImmediateRequest);

    extension->TransmitImmediate = TRUE;
    extension->ImmediateChar = *((UCHAR*) (reqContext->SystemBuffer));

    // The isr now has a reference to the request.

    SERIAL_SET_REFERENCE(reqContext,
                         SERIAL_REF_ISR);

    // Check first to see if a write is going on.  If
    // there is then we'll just slip in during the write.

    if (!extension->WriteLength) {

        // If there is no normal write transmitting then we
        // will "re-enable" the transmit holding register empty
        // interrupt.  The 8250 family of devices will always
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
            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INTERRUPT, 
                        "SerialGiveImmediateToIsr() - disable-enable both interrupts\r\n");
        }
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INTERRUPT, "--SerialGiveImmediateToIsr()\r\n");
    return FALSE;
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
SerialGrabImmediateFromIsr(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = (PSERIAL_DEVICE_EXTENSION)Context;
    PREQUEST_CONTEXT reqContext;

    UNREFERENCED_PARAMETER(Interrupt);

    reqContext = SerialGetRequestContext(extension->CurrentImmediateRequest);

    if (extension->TransmitImmediate) {

        extension->TransmitImmediate = FALSE;

        // Since the isr no longer references this request, we can
        // decrement it's reference count.

        SERIAL_CLEAR_REFERENCE(reqContext,
                                SERIAL_REF_ISR);
    }

    return FALSE;
}


