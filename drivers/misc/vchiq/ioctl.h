//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     ioctl.h
//
// Abstract:
//
//     This file contains the ioctl header for vchiq driver.
//

#pragma once

EXTERN_C_START

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL VchiqIoDeviceControl;

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqUpdateQueueDispatchMessage (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_opt_ WDFREQUEST Request,
    _In_opt_ WDFQUEUE MsgQueue,
    _In_ ULONG MessageId,
    _In_reads_bytes_(BufferSize) VOID* BufferPtr,
    _In_ ULONG BufferSize
    );

EVT_WDF_IO_IN_CALLER_CONTEXT VchiqInCallerContext;

EXTERN_C_END