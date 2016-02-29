//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    device.h
//
// Abstract:
//
//    This file contains the device definitions.
//

#pragma once

EXTERN_C_START

#define RPIQ_VERSION_MAJOR   0
#define RPIQ_VERSION_MINOR   1

#define RPIQ_NONPAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push))

#define RPIQ_NONPAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define RPIQ_PAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("PAGE"))

#define RPIQ_PAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define RPIQ_INIT_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("INIT"))

#define RPIQ_INIT_SEGMENT_END \
    __pragma(code_seg(pop))

#define RPIQ_MEMORY_RESOURCE_TOTAL  1
#define RPIQ_INT_RESOURCE_TOTAL     1

extern const int RpiqTag;

typedef struct _DEVICE_CONTEXT {
    // Version
    ULONG VersionMajor;
    ULONG VersionMinor;
    
    // Mailbox Register
    MAILBOX* Mailbox;
    ULONG MailboxMmioLength;

    // Lock
    WDFWAITLOCK WriteLock;

    // Mailbox channel wdf queue object
    WDFQUEUE ChannelQueue[MAILBOX_CHANNEL_MAX];
    
    // Interrupt
    WDFINTERRUPT MailboxIntObj;
    
    // Device interface notification handle
    VOID* NdisNotificationHandle;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, RpiqGetContext)

EVT_WDF_DEVICE_PREPARE_HARDWARE RpiqPrepareHardware;

EVT_WDF_DEVICE_RELEASE_HARDWARE RpiqReleaseHardware;

DRIVER_NOTIFICATION_CALLBACK_ROUTINE RpiqNdisInterfaceCallback;

EVT_WDF_IO_TARGET_REMOVE_COMPLETE RpiqNdisTargetRemoveComplete;

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
RpiqCreateDevice (
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

EVT_WDF_IO_QUEUE_IO_STOP RpiqIoStop;

EXTERN_C_END