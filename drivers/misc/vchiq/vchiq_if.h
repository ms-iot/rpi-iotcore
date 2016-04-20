//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     vchiq_if.h
//
// Abstract:
//
//     This header contains sepcific definition from Raspberry Pi
//     public userland repo. This is based on the vchiq_if.h header.
//     Only minimal definitions are ported over.
//

#pragma once

EXTERN_C_START

#define VCHIQ_MAX_MSG_SIZE  (VCHIQ_SLOT_SIZE - sizeof(VCHIQ_HEADER))
#define VCHIQ_CHANNEL_SIZE  VCHIQ_MAX_MSG_SIZE /* For backwards compatibility */
#define VCHIQ_MAKE_FOURCC(x0, x1, x2, x3) \
            (((x0) << 24) | ((x1) << 16) | ((x2) << 8) | (x3))

typedef enum {
    VCHIQ_SERVICE_OPENED,         /* service, -, -             */
    VCHIQ_SERVICE_CLOSED,         /* service, -, -             */
    VCHIQ_MESSAGE_AVAILABLE,      /* service, header, -        */
    VCHIQ_BULK_TRANSMIT_DONE,     /* service, -, bulk_userdata */
    VCHIQ_BULK_RECEIVE_DONE,      /* service, -, bulk_userdata */
    VCHIQ_BULK_TRANSMIT_ABORTED,  /* service, -, bulk_userdata */
    VCHIQ_BULK_RECEIVE_ABORTED    /* service, -, bulk_userdata */
} VCHIQ_REASON_T;

typedef enum {
    VCHIQ_ERROR = -1,
    VCHIQ_SUCCESS = 0,
    VCHIQ_RETRY = 1
} VCHIQ_STATUS_T;

typedef enum {
    VCHIQ_BULK_MODE_CALLBACK,
    VCHIQ_BULK_MODE_BLOCKING,
    VCHIQ_BULK_MODE_NOCALLBACK,
    VCHIQ_BULK_MODE_WAITING        /* Reserved for internal use */
} VCHIQ_BULK_MODE_T;

typedef enum {
    VCHIQ_SERVICE_OPTION_AUTOCLOSE,
    VCHIQ_SERVICE_OPTION_SLOT_QUOTA,
    VCHIQ_SERVICE_OPTION_MESSAGE_QUOTA,
    VCHIQ_SERVICE_OPTION_SYNCHRONOUS,
    VCHIQ_SERVICE_OPTION_TRACE
} VCHIQ_SERVICE_OPTION_T;

#include <pshpack1.h>

typedef struct _VCHIQ_HEADER {
    /* The message identifier - opaque to applications. */
    ULONG MsgId;

    /* Size of message data. */
    ULONG Size;

    /* message */
} VCHIQ_HEADER, *PVCHIQ_HEADER;

typedef struct _VCHIQ_ELEMENT {
    VOID* Data;
    ULONG Size;
    WDFMEMORY WdfMemoryData;
} VCHIQ_ELEMENT, *PVCHIQ_ELEMENT;

typedef unsigned int VCHIQ_SERVICE_HANDLE;

typedef VCHIQ_STATUS_T (*VCHIQ_CALLBACK_T)(VCHIQ_REASON_T, VCHIQ_HEADER *,
    VCHIQ_SERVICE_HANDLE, void *);

typedef struct _VCHIQ_SERVICE_BASE {
    int FourCC;
    VCHIQ_CALLBACK_T Callback;
    VOID* Userdata;
} VCHIQ_SERVICE_BASE, *PVCHIQ_SERVICE_BASE;

typedef struct _VCHIQ_SERVICE_PARAMS {
    int FourCC;
    VCHIQ_CALLBACK_T Callback;
    VOID* UserData;
    USHORT Version;       /* Increment for non-trivial changes */
    USHORT VersionMin;    /* Update for incompatible changes */
} VCHIQ_SERVICE_PARAMS, *PVCHIQ_SERVICE_PARAMS;

typedef struct _VCHIQ_CONFIG {
    unsigned int MaxMsgSize;
    unsigned int BulkThreshold; /* The message size above which it
                                 is better to use a bulk transfer
                                 (<= max_msg_size) */
    unsigned int MaxOutstandingBulks;
    unsigned int MaxServices;
    short Version;      /* The version of VCHIQ */
    short VersionMin;  /* The minimum compatible version of VCHIQ */
} VCHIQ_CONFIG, *PVCHIQ_CONFIG;

#include <poppack.h>

EXTERN_C_END