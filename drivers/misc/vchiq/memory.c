//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//      memory.c
//
// Abstract:
//
//        Memory related implementation including alloc and free
//

#include "precomp.h"

#include "trace.h"
#include "memory.tmh"

#include "slotscommon.h"
#include "device.h"
#include "file.h"
#include "memory.h"

VCHIQ_PAGED_SEGMENT_BEGIN

/*++

Routine Description:

    Allocates physical contiguous memory

Arguments:

    DeviceContextPtr - Pointer to device context

    BufferSize - Size of buffer to be allocated

    BufferPPtr - Pointer to the allocated buffer

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS VchiqAllocPhyContiguous (
    DEVICE_CONTEXT* DeviceContextPtr,
    ULONG BufferSize,
    VOID** BufferPPtr
    )
{
    NTSTATUS status;
    PHYSICAL_ADDRESS highAddress;
    PHYSICAL_ADDRESS lowAddress = { 0 };
    PHYSICAL_ADDRESS boundaryAddress = { 0 };

    PAGED_CODE();

    if (BufferPPtr == NULL || BufferSize == 0) {
        status = STATUS_INVALID_PARAMETER;
        goto End;
    }

    highAddress.QuadPart = MEMORY_SIZE_1_G;

    *BufferPPtr = MmAllocateContiguousNodeMemory(
        BufferSize,
        lowAddress,
        highAddress,
        boundaryAddress,
        PAGE_NOCACHE | PAGE_READWRITE,
        MM_ANY_NODE_OK);
    if (*BufferPPtr == NULL) {
        VCHIQ_LOG_ERROR("Fail to allocated contiguous memory");
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto End;
    }

    ++DeviceContextPtr->AllocPhyMemCount;
    RtlZeroMemory(*BufferPPtr, BufferSize);

    status = STATUS_SUCCESS;

End:
    return status;
}

/*++

Routine Description:

    Free physical contiguous memory that was previously allocated

Arguments:

    DeviceContextPtr - Pointer to device context

    BufferPPtr - Pointer to the buffer to be freed

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS VchiqAllocateCommonBuffer (
    VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    ULONG BufferSize,
    VOID** BufferPPtr,
    PHYSICAL_ADDRESS* PhyAddressPtr
    )
{
    NTSTATUS status;
    DMA_ADAPTER* dmaAdapterPtr = VchiqFileContextPtr->DmaAdapterPtr;

    PAGED_CODE();

    if (BufferPPtr == NULL || BufferSize == 0) {
        status = STATUS_INVALID_PARAMETER;
        goto End;
    }

    *BufferPPtr = dmaAdapterPtr->DmaOperations->AllocateCommonBuffer(
        dmaAdapterPtr,
        BufferSize,
        PhyAddressPtr,
        FALSE);
    if (*BufferPPtr == NULL) {
        VCHIQ_LOG_ERROR("Fail to allocated common memory");
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto End;
    }

    status = STATUS_SUCCESS;

End:
    return status;
}

VCHIQ_PAGED_SEGMENT_END

VCHIQ_NONPAGED_SEGMENT_BEGIN

/*++

Routine Description:

    Free physical contiguous memory that was previously allocated

Arguments:

    VchiqFileContextPtr - File context pointer returned to caller

    BufferPPtr - Pointer to the buffer to be freed

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS VchiqFreeCommonBuffer (
    VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    ULONG BufferSize,
    PHYSICAL_ADDRESS PhyAddress,
    VOID* BufferPtr
    )
{
    NTSTATUS status;
    DMA_ADAPTER* dmaAdapterPtr = VchiqFileContextPtr->DmaAdapterPtr;

    if (BufferPtr == NULL || BufferSize == 0) {
        status = STATUS_INVALID_PARAMETER;
        goto End;
    }

    dmaAdapterPtr->DmaOperations->FreeCommonBuffer(
        dmaAdapterPtr,
        BufferSize,
        PhyAddress,
        BufferPtr,
        FALSE);

    status = STATUS_SUCCESS;

End:
    return status;
}

/*++

Routine Description:

    Free physical contiguous memory that was previously allocated

Arguments:

    DeviceContextPtr - Pointer to device context

    BufferPPtr - Pointer to the buffer to be freed

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS VchiqFreePhyContiguous (
    DEVICE_CONTEXT* DeviceContextPtr,
    VOID** BufferPPtr
    )
{
    NTSTATUS status;

    NT_ASSERT(DeviceContextPtr->AllocPhyMemCount != 0);

    if (*BufferPPtr == NULL) {
        status = STATUS_INVALID_PARAMETER;
        goto End;
    }

    MmFreeContiguousMemory(*BufferPPtr);
    --DeviceContextPtr->AllocPhyMemCount;

    *BufferPPtr = NULL;
    status = STATUS_SUCCESS;

End:
    return status;
}

VCHIQ_NONPAGED_SEGMENT_END