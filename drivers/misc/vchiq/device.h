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

#define VCHIQ_VERSION_MAJOR   0
#define VCHIQ_VERSION_MINOR   1

#define VCHIQ_NONPAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push))

#define VCHIQ_NONPAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define VCHIQ_PAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("PAGE"))

#define VCHIQ_PAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define VCHIQ_INIT_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("INIT"))

#define VCHIQ_INIT_SEGMENT_END \
    __pragma(code_seg(pop))

#define VCHIQ_MEMORY_RESOURCE_TOTAL  1
#define VCHIQ_INT_RESOURCE_TOTAL     1
#define MAX_ARM_PORTS 4096
#define ARM_PORT_START 1

enum ULONG {
    // Temporary be freed in the same routine
    VCHIQ_ALLOC_TAG_TEMP = '0QHV', 
   
    // Lookaside allocation for pending messages
    VCHIQ_ALLOC_TAG_PENDING_MSG = '1QHV',

    // Lookaside allocation for bulk transaction
    VCHIQ_ALLOC_TAG_PENDING_BULK_MSG = '2QHV' ,

    // Global objecst
    VCHIQ_ALLOC_TAG_GLOBAL_OBJ = '3QHV',

    // Generic allocations WDF makes on our behalf
    VCHIQ_ALLOC_TAG_WDF = '@QHV'
};

enum {
    // Thread types
    THREAD_TRIGGER = 0,
    THREAD_RECYCLE,
    THREAD_SYNC,
    // Only support trigger and recycle for now
    THREAD_MAX_SUPPORTED = 2,
    THREAD_SYNC_RELEASE,
    THREAD_TOTAL,
};

typedef struct _DEVICE_CONTEXT {
    // Version
    ULONG VersionMajor;
    ULONG VersionMinor;

    // Device object
    WDFDEVICE Device;
    DEVICE_OBJECT* PhyDeviceObjectPtr;

    // VCHIQ register
    UCHAR* VchiqRegisterPtr;
    ULONG VchiqRegisterLength;

    // Interrupt
    WDFINTERRUPT VchiqIntObj;

    // VCHIQ Slot
    VCHIQ_SLOT_ZERO* SlotZeroPtr;
    PHYSICAL_ADDRESS SlotMemoryPhy;
    FAST_MUTEX TxSlotMutex;
    FAST_MUTEX RecycleSlotMutex;

    // Slot state, these should only be access when TxSlotMutex is acquired
    UCHAR* MasterCurrentSlot;
    ULONG MasterCurrentSlotIndex;
    UCHAR* SlaveCurrentSlot;
    BOOLEAN VCConnected;
    BOOLEAN DeviceInterfaceEnabled;

    // Tx slot info
    ULONG CurrentTxPos;
    ULONG RecycleTxSlotIndex;
    KSEMAPHORE AvailableTxSlot;
    volatile LONG AvailableTxSlotCount;

    // Rx slot info
    ULONG CurrentRxPos;
    SLOT_INFO RxSlotInfo[VCHIQ_MAX_SLOTS];

    //VHIQ system thread
    KEVENT VchiqThreadEvent[THREAD_TOTAL];
    HANDLE VchiqThreadHandle[THREAD_TOTAL];
    PKTHREAD VchiqThreadObj[THREAD_TOTAL];
    KEVENT VchiqThreadEventStop;

    // Device interface notification handle
    VOID* RpiqNotificationHandle;

    // stash all the active file object handles
    // into the port array port[array] that is zero means unused port number
    VOID* ArmPortHandles[MAX_ARM_PORTS];

    // Physical memory counter
    ULONG AllocPhyMemCount;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, VchiqGetDeviceContext)

// Device Callback
EVT_WDF_DEVICE_PREPARE_HARDWARE VchiqPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE VchiqReleaseHardware;
EVT_WDF_IO_QUEUE_IO_STOP VchiqIoStop;

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
VchiqCreateDevice (
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInitPtr
    );

EXTERN_C_END