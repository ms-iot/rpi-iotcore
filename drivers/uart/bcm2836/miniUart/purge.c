// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//   purge.c
//
// Abstract:
//
//    This module contains the code that is very specific to purge
//    operations in the serial driver
//

#include "precomp.h"
#include "purge.tmh"

/*++

Routine Description:

    Depending on the mask in the current request, purge the interrupt
    buffer, the read queue, or the write queue, or all of the above.

Arguments:

    Extension - Pointer to the device extension.

Return Value:

    Will return STATUS_SUCCESS always.  This is reasonable
    since the DPC completion code that calls this routine doesn't
    care and the purge request always goes through to completion
    once it's started.

--*/
_Use_decl_annotations_
VOID
SerialStartPurge(
    PSERIAL_DEVICE_EXTENSION Extension
    )
{
    WDFREQUEST NewRequest;
    PREQUEST_CONTEXT reqContext;

    do {

        ULONG Mask;
        reqContext = SerialGetRequestContext(Extension->CurrentPurgeRequest);
        Mask = *((ULONG*) (reqContext->SystemBuffer));

        if (Mask & SERIAL_PURGE_TXABORT) {

            SerialFlushRequests(Extension->WriteQueue,
                                &Extension->CurrentWriteRequest);

            SerialFlushRequests(Extension->WriteQueue,
                                &Extension->CurrentXoffRequest);

        }

        if (Mask & SERIAL_PURGE_RXABORT) {

            SerialFlushRequests(Extension->ReadQueue,
                                &Extension->CurrentReadRequest);

        }

        if (Mask & SERIAL_PURGE_RXCLEAR) {

            // Clean out the interrupt buffer.
            //
            // Note that we do this under protection of the
            // the drivers control lock so that we don't hose
            // the pointers if there is currently a read that
            // is reading out of the buffer.

            WdfInterruptSynchronize(Extension->WdfInterrupt,
                                    SerialPurgeInterruptBuff,
                                    Extension);
        }

        reqContext->Status = STATUS_SUCCESS;
        reqContext->Information = 0;

        SerialGetNextRequest(&Extension->CurrentPurgeRequest,
                            Extension->PurgeQueue,
                            &NewRequest,
                            TRUE,
                            Extension);

    } while (NewRequest);

    return;
}

/*++

Routine Description:

    This routine simply resets the interrupt (typeahead) buffer.

    NOTE: This routine is being called from WdfInterruptSynchronize.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    Always false.

--*/
_Use_decl_annotations_
BOOLEAN
SerialPurgeInterruptBuff(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = Context;

    UNREFERENCED_PARAMETER(Interrupt);

    // The typeahead buffer is by definition empty if there
    // currently is a read owned by the isr.

    if (extension->ReadBufferBase == extension->InterruptReadBuffer) {

        extension->CurrentCharSlot = extension->InterruptReadBuffer;
        extension->FirstReadableChar = extension->InterruptReadBuffer;
        extension->LastCharSlot = extension->InterruptReadBuffer +
                                      (extension->BufferSize - 1);
        extension->CharsInInterruptBuffer = 0;

        SerialHandleReducedIntBuffer(extension);

    }

    return FALSE;
}


