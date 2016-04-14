//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//      transfer.c
//
// Abstract:
//
//        VCHIQ bulk transfer related implementation
//

#include "precomp.h"

#include "trace.h"
#include "transfer.tmh"

#include "slotscommon.h"
#include "device.h"
#include "file.h"
#include "slots.h"
#include "memory.h"
#include "transfer.h"

VCHIQ_PAGED_SEGMENT_BEGIN

/*++

Routine Description:

    VchiqAllocateTransferRequestObjContext would allocated
        a context for the current TX request.

Arguments:

    DeviceContextPtr - Device context pointer

    VchiqFileContextPtr - File context pointer returned to caller

    WdfRequest - A handle to a framework request object for vchiq transfer

    BufferMdlPtr - Pointer to the mdl buffer allocated to perform the transfer

    PageListPtr - Page list pointer to the buffer transfer

    PageListSize - Page list size
    
    PageListPhyAddr - Page list physical address

    ScatterGatherListPtr - Scatter gather list allocated for this transfer

    VchiqTxRequestContextPPtr - TX request context allocatec by the function
          and returned to caller

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS VchiqAllocateTransferRequestObjContext (
    DEVICE_CONTEXT* DeviceContextPtr,
    VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    WDFREQUEST WdfRequest,
    MDL* BufferMdlPtr,
    VOID* PageListPtr,
    ULONG PageListSize,
    PHYSICAL_ADDRESS PageListPhyAddr,
    SCATTER_GATHER_LIST* ScatterGatherListPtr,
    VCHIQ_TX_REQUEST_CONTEXT** VchiqTxRequestContextPPtr
    )
{
    WDF_OBJECT_ATTRIBUTES wdfObjectAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &wdfObjectAttributes,
        VCHIQ_TX_REQUEST_CONTEXT);
    wdfObjectAttributes.EvtCleanupCallback =
        VchiqTransferRequestContextCleanup;

    PAGED_CODE();

    NTSTATUS status = WdfObjectAllocateContext(
        WdfRequest,
        &wdfObjectAttributes,
        VchiqTxRequestContextPPtr);
    if (!NT_SUCCESS(status)) {
        VCHIQ_LOG_WARNING(
            "WdfObjectAllocateContext() failed %!STATUS!)",
            status);
        goto End;
    }

    (*VchiqTxRequestContextPPtr)->BufferMdlPtr = BufferMdlPtr;
    (*VchiqTxRequestContextPPtr)->PageListPtr = PageListPtr;
    (*VchiqTxRequestContextPPtr)->PageListSize = PageListSize;
    (*VchiqTxRequestContextPPtr)->PageListPhyAddr = PageListPhyAddr;
    (*VchiqTxRequestContextPPtr)->ScatterGatherListPtr = ScatterGatherListPtr;
    (*VchiqTxRequestContextPPtr)->DeviceContextPtr = DeviceContextPtr;
    (*VchiqTxRequestContextPPtr)->VchiqFileContextPtr = VchiqFileContextPtr;

End:
    return status;
}

VCHIQ_PAGED_SEGMENT_END

VCHIQ_NONPAGED_SEGMENT_BEGIN

/*++

Routine Description:

    VchiqTransferRequestContextCleanup would perform cleanup
        when request object is delete.

Arguments:

    WdfObject - A handle to a framework object in this case
         a WDFRequest

Return Value:

    VOID

--*/
_Use_decl_annotations_
VOID VchiqTransferRequestContextCleanup (
    WDFOBJECT WdfObject
    )
{
    VCHIQ_TX_REQUEST_CONTEXT* vchiqTxRequestContextPtr =
        VchiqGetTxRequestContext(WdfObject);
    VCHIQ_FILE_CONTEXT* vchiqFileContextPtr =
        vchiqTxRequestContextPtr->VchiqFileContextPtr;
    
    if (vchiqTxRequestContextPtr->BufferMdlPtr) {
        vchiqTxRequestContextPtr->BufferMdlPtr = NULL;
    }

    if (vchiqTxRequestContextPtr->PageListPtr) {
        VchiqFreeCommonBuffer(
            vchiqFileContextPtr,
            vchiqTxRequestContextPtr->PageListSize,
            vchiqTxRequestContextPtr->PageListPhyAddr,
            vchiqTxRequestContextPtr->PageListPtr);
        vchiqTxRequestContextPtr->PageListPtr = NULL;
    }

    if (vchiqTxRequestContextPtr->ScatterGatherListPtr) {

        vchiqFileContextPtr->DmaAdapterPtr->DmaOperations->FreeAdapterObject(
            vchiqFileContextPtr->DmaAdapterPtr,
            DeallocateObjectKeepRegisters);

        vchiqFileContextPtr->DmaAdapterPtr->DmaOperations->PutScatterGatherList(
            vchiqFileContextPtr->DmaAdapterPtr,
            vchiqTxRequestContextPtr->ScatterGatherListPtr,
            FALSE);

        vchiqTxRequestContextPtr->ScatterGatherListPtr = NULL;
    }

    return;
}

VCHIQ_NONPAGED_SEGMENT_END