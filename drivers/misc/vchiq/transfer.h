//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     transfer.h
//
// Abstract:
//
//     This file contains vchiq bulk transfer related definitions.
//

#pragma once

EXTERN_C_START

typedef struct _VCHIQ_TX_REQUEST_CONTEXT {
     MDL* BufferMdlPtr;
     VOID* PageListPtr;
     ULONG PageListSize;
     PHYSICAL_ADDRESS PageListPhyAddr;
     SCATTER_GATHER_LIST* ScatterGatherListPtr;
     DEVICE_CONTEXT* DeviceContextPtr;
     VCHIQ_FILE_CONTEXT* VchiqFileContextPtr;
} VCHIQ_TX_REQUEST_CONTEXT, *PVCHIQ_TX_REQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    VCHIQ_TX_REQUEST_CONTEXT, 
    VchiqGetTxRequestContext);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqAllocateTransferRequestObjContext (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ WDFREQUEST Request,
    _In_ MDL* BufferMdlPtr,
    _In_ VOID* PageListPtr,
    _In_ ULONG PageListSize,
    _In_ PHYSICAL_ADDRESS PageListPhyAddr,
    _In_ SCATTER_GATHER_LIST* ScatterGatherListPtr,
    _Outptr_ VCHIQ_TX_REQUEST_CONTEXT** VchiqTxRequestContextPPtr
    );

EVT_WDF_OBJECT_CONTEXT_CLEANUP VchiqTransferRequestContextCleanup;

EXTERN_C_END
