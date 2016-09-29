// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    error.c
// Abstract:
//
//    This module contains the code that is very specific to error
//    operations in the serial driver

#include "precomp.h"
#include "error.tmh"

/*++

Routine Description:

    This routine is invoked at dpc level to in response to
    a comm error.  All comm errors complete all read and writes

Arguments:

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialCommError(
    WDFDPC Dpc
    )
{
    PSERIAL_DEVICE_EXTENSION extension = NULL;

    extension = SerialGetDeviceExtension(WdfDpcGetParentObject(Dpc));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT,
                "++SerialCommError(%p)\r\n",
                extension);

    SerialFlushRequests(extension->WriteQueue,
                        &extension->CurrentWriteRequest);

    SerialFlushRequests(extension->ReadQueue,
                        &extension->CurrentReadRequest);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT,
                "--SerialCommError\r\n");
}


