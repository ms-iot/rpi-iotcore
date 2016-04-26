//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     vchiq_ioctl.h
//
// Abstract:
//
//     This header contains sepcific definition from Raspberry Pi
//     public userland repo. This is based on the vchiq_ioctl.h header
//

#pragma once

EXTERN_C_START

#include <pshpack1.h>

#define VCHIQ_IOC_MAGIC 0xc4
#define VCHIQ_INVALID_HANDLE (~0)

typedef struct _VCHIQ_CREATE_SERVICE {
    VCHIQ_SERVICE_PARAMS Params;
    int IsOpen;
    int IsVchi;
    unsigned int handle;       /* OUT */
} VCHIQ_CREATE_SERVICE, *PVCHIQ_CREATE_SERVICE;

typedef struct _VCHIQ_QUEUE_MESSAGE {
    unsigned int Handle;
    unsigned int Count;
    VCHIQ_ELEMENT* Elements;
    WDFMEMORY WdfMemoryElementBuffer;
} VCHIQ_QUEUE_MESSAGE, *PVCHIQ_QUEUE_MESSAGE;

typedef struct _VCHIQ_QUEUE_BULK_TRANSFER {
    unsigned int Handle;
    VOID* Data;
    unsigned int Size;
    VOID* UserData;
    VCHIQ_BULK_MODE_T Mode;
    WDFMEMORY WdfMemoryBuffer;
} VCHIQ_QUEUE_BULK_TRANSFER, *PVCHIQ_QUEUE_BULK_TRANSFER;

typedef struct VCHIQ_COMPLETION_DATA {
    VCHIQ_REASON_T Reason;
    VCHIQ_HEADER* Header;
    VOID* ServiceUserData;
    VOID* BulkUserData;
    WDFMEMORY WdfMemoryBuffer;
} VCHIQ_COMPLETION_DATA, *PVCHIQ_COMPLETION_DATA;

typedef struct _VCHIQ_AWAIT_COMPLETION {
    unsigned int Count;
    VCHIQ_COMPLETION_DATA* Buf;
    unsigned int MsgBufSize;
    unsigned int MsgBufCount; /* IN/OUT */
    VOID** MsgBufs;
    WDFMEMORY WdfMemoryCompletion;
} VCHIQ_AWAIT_COMPLETION, *PVCHIQ_AWAIT_COMPLETION;

typedef struct _VCHIQ_DEQUEUE_MESSAGE {
    unsigned int Handle;
    int Blocking;
    unsigned int BufSize;
    VOID* Buf;
    WDFMEMORY WdfMemoryBuffer;
} VCHIQ_DEQUEUE_MESSAGE, *PVCHIQ_DEQUEUE_MESSAGE;


typedef struct _VCHIQ_GET_CONFIG {
    unsigned int ConfigSize;
    VCHIQ_CONFIG* PConfig;
    WDFMEMORY WdfMemoryConfiguration;
} VCHIQ_GET_CONFIG, *PVCHIQ_GET_CONFIG;

typedef struct _VCHIQ_SET_SERVICE_OPTION {
    unsigned int Handle;
    VCHIQ_SERVICE_OPTION_T Option;
    int Value;
} VCHIQ_SET_SERVICE_OPTION, *PVCHIQ_SET_SERVICE_OPTION;

typedef struct _VCHIQ_DUMP_MEM {
    VOID* VirtAddr;
    size_t NumBytes;
} VCHIQ_DUMP_MEM, *PVCHIQ_DUMP_MEM;

#include <poppack.h> 

EXTERN_C_END