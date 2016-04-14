//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     file.h
//
// Abstract:
//
//     This file contains file handle related definitions.
//

#pragma once

EXTERN_C_START

enum {
    // Message queues
    FILE_QUEUE_CREATE_SERVICE = 0,
    FILE_QUEUE_CLOSE_SERVICE,
    FILE_QUEUE_PENDING_MSG,
    FILE_QUEUE_PENDING_VCHI_MSG,
    FILE_QUEUE_TX_DATA,
    FILE_QUEUE_RX_DATA,
    FILE_QUEUE_MAX,
};

typedef enum _MSG_BULK_TYPE {
    // Message queues
    MSG_BULK_TX = 0,
    MSG_BULK_RX,
    MSG_BULK_MAX,
}MSG_BULK_TYPE;

typedef enum _SERVICE_STATE {
    // Service State
    SERVICE_STATE_MIN = 0,
    SERVICE_STATE_OPEN = 1,
    SERVICE_STATE_CLOSE = 2,
}_ERVICE_STATE;

typedef struct _VCHIQ_FILE_CONTEXT {
    ULONG    ArmPortNumber;
    ULONG    VCHIQPortNumber;
    
    // Lookaside memory per file handle to take advantage
    // of WDF memory cleanup by parenting to file object
    WDFLOOKASIDE PendingMsgLookAsideMemory;
    WDFLOOKASIDE PendingBulkMsgLookAsideMemory;

    LIST_ENTRY PendingDataMsgList;
    FAST_MUTEX PendingDataMsgMutex;
    
    LIST_ENTRY PendingBulkMsgList[MSG_BULK_MAX];
    FAST_MUTEX PendingBulkMsgMutex[MSG_BULK_MAX];

    WDFQUEUE FileQueue[FILE_QUEUE_MAX];

    KEVENT FileEventStop;
    
    // Pointer to service data in user space. Userland expects the driver 
    // returns this back when completing a transaction
    VOID *ServiceUserData;

    ULONG IsVchi;
    LIST_ENTRY PendingVchiMsgList;
    FAST_MUTEX PendingVchiMsgMutex;

    // Minimal state management for now. Consider to expand more service
    // state tracking i current implementation is insufficient.
    volatile LONG State;

    // DMA
    DMA_ADAPTER* DmaAdapterPtr;

} VCHIQ_FILE_CONTEXT, *PVCHIQ_FILE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(VCHIQ_FILE_CONTEXT, VchiqGetFileContext);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqAllocateFileObjContext (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ WDFFILEOBJECT WdfFileObject,
    _Outptr_ VCHIQ_FILE_CONTEXT** VchiqFileContextPPtr
    );

EVT_WDF_FILE_CLOSE VchiqFileClose;

EXTERN_C_END