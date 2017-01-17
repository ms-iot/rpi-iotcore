//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    Mailbox.h
//
// Abstract:
//
//    Mailbox Interface.
//

#pragma once

EXTERN_C_START

#define MAX_POLL        50

typedef struct _RPIQ_REQUEST_CONTEXT {
    VOID* PropertyMemory;
    ULONG PropertyMemorySize;
} RPIQ_REQUEST_CONTEXT, *PRPIQ_REQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    RPIQ_REQUEST_CONTEXT,
    RpiqGetRequestContext);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS RpiqMailboxInit (
    _In_ WDFDEVICE Device
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS RpiqMailboxWrite (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ ULONG Channel,
    _In_ ULONG Value,
    _In_opt_ WDFREQUEST Request
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS RpiqMailboxProperty (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_reads_bytes_(DataSize) const VOID* DataInPtr,
    _In_ ULONG DataSize,
    _In_ ULONG Channel,
    _In_ WDFREQUEST Request
    );

EVT_WDF_OBJECT_CONTEXT_CLEANUP RpiqRequestContextCleanup;

EXTERN_C_END
