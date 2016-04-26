//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     slots.h
//
// Abstract:
//
//     Vchiq slot header definition.
//

#pragma once

EXTERN_C_START

// Slot information

// The current default slot number
//   Slot zero - Takes up 1 slot
//   Rx slot   - 32 slot in total
//   Tx slot   - 32 slot in total
#define VCHIQ_DEFAULT_TOTAL_SLOT_ZERO   1
#define VCHIQ_DEFAULT_TOTAL_RX_SLOTS    32
#define VCHIQ_DEFAULT_TOTAL_TX_SLOTS    32
#define VCHIQ_DEFAULT_TOTAL_SLOTS (VCHIQ_DEFAULT_TOTAL_SLOT_ZERO + \
    VCHIQ_DEFAULT_TOTAL_RX_SLOTS + VCHIQ_DEFAULT_TOTAL_TX_SLOTS)

// Slot message size needs to be 8 byte aligned
#define SLOT_MSG_SIZE_ALIGN            8
#define SLOT_MSG_SIZE_MASK_ALIGN     (SLOT_MSG_SIZE_ALIGN - 1)

#define VCHIQ_GET_CURRENT_TX_HEADER(a) \
     (VCHIQ_HEADER*)((UCHAR*)a->SlaveCurrentSlot + \
          (a->CurrentTxPos & VCHIQ_SLOT_MASK))

#define VCHIQ_GET_NEXT_TX_SLOT_INDEX(a) \
     a->SlotZeroPtr->Slave.SlotQueue[ \
          ((a->CurrentTxPos / VCHIQ_SLOT_SIZE) & VCHIQ_SLOT_QUEUE_MASK)]

#define VCHIQ_GET_CURRENT_RX_HEADER(a) \
     (VCHIQ_HEADER*)((UCHAR*)a->MasterCurrentSlot + \
          (a->CurrentRxPos & VCHIQ_SLOT_MASK))

#define VCHIQ_GET_NEXT_RX_SLOT_INDEX(a) \
     a->SlotZeroPtr->Master.SlotQueue[ \
          ((a->CurrentRxPos / VCHIQ_SLOT_SIZE) & VCHIQ_SLOT_QUEUE_MASK)]

#define VCHIQ_GET_HEADER_BY_LOCAL_POS(slot, pos) \
     (VCHIQ_HEADER*)((UCHAR*)slot + (pos & VCHIQ_SLOT_MASK))

#define VCHIQ_GET_HEADER_BY_LOCAL_INDEX(slot, index) \
     (VCHIQ_HEADER*)(slot + index)

#define VCHIQ_GET_HEADER_BY_GLOBAL_INDEX(a, index) \
     (VCHIQ_HEADER*)((VCHIQ_SLOT*)a->SlotZeroPtr + index)

#define VCHIQ_MSG_CONNECT_SIZE        sizeof(VCHIQ_HEADER)

__inline ULONG VCHIQ_GET_SLOT_ALIGN_SIZE (
    _In_ ULONG Size
    )
{
    return (Size + SLOT_MSG_SIZE_MASK_ALIGN) & (~SLOT_MSG_SIZE_MASK_ALIGN);
}

__inline VOID VCHIQ_ENABLE_EVENT_INTERRUPT (
    _In_ VCHIQ_REMOTE_EVENT* EventPtr
    )
{
    EventPtr->Armed = 1;
}

__inline VOID VCHIQ_RESET_EVENT_SIGNAL (
    _In_ VCHIQ_REMOTE_EVENT* EventPtr
    )
{
    // Reset the 'Fired' state so we can check this value to detect 
    // any notification that the firmware may have fired before we have a 
    // chance to armed the interrupt for the firmware.
    EventPtr->Fired = 0;
}

__inline BOOLEAN VCHIQ_IS_EVENT_SIGNAL (
    _In_ VCHIQ_REMOTE_EVENT* EventPtr
    )
{
    // Check if if the firmware has attempted to notify of a new message
    // before we enabled interrupt. Resetting the 'Fired' state before
    // enabling the interrupt is the only way to stay in sync with firmware
    return (EventPtr->Fired == 1);
}

__inline WCHAR* VCHIQ_MESSAGE_NAME (
    _In_ ULONG MessageId
    )
{
    switch (VCHIQ_MSG_TYPE(MessageId))
    {
    case VCHIQ_MSG_PADDING:
        return L"PADDING";
    case VCHIQ_MSG_CONNECT:
        return L"CONNECT";
    case VCHIQ_MSG_OPEN:
        return L"OPEN";
    case VCHIQ_MSG_OPENACK:
        return L"OPENACK";
    case VCHIQ_MSG_CLOSE:
        return L"CLOSE";
    case VCHIQ_MSG_DATA:
        return L"DATA";
    case VCHIQ_MSG_BULK_RX:
        return L"BULK_RX";
    case VCHIQ_MSG_BULK_TX:
        return L"BULK_TX";
    case VCHIQ_MSG_BULK_RX_DONE:
        return L"BULK_RX_DONE";
    case VCHIQ_MSG_BULK_TX_DONE:
        return L"BULK_TX_DONE";
    case VCHIQ_MSG_PAUSE:
        return L"PAUSE";
    case VCHIQ_MSG_RESUME:
        return L"RESUME";
    case VCHIQ_MSG_REMOTE_USE:
        return L"REMOTE_USE";
    case VCHIQ_MSG_REMOTE_RELEASE:
        return L"REMOTE_RELEASE";
    case VCHIQ_MSG_REMOTE_USE_ACTIVE:
        return L"REMOTE_USE_ACTIVE";
    default:
        return L"Unknown";
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqInit (
    _In_ DEVICE_CONTEXT* DeviceContextPtr
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqRelease (
    _In_ DEVICE_CONTEXT* DeviceContextPtr
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqSignalVC (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_REMOTE_EVENT* EventPtr
    );

_Requires_lock_held_(&DeviceContextPtr->TxSlotMutex)
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqAcquireTxSpace (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ ULONG RequestSize,
    _In_ BOOLEAN SyncAcquire,
    _Outptr_ VCHIQ_HEADER** HeaderPPtr
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqProcessRxSlot (
    _In_ DEVICE_CONTEXT* DeviceContextPtr
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqProcessRecycleTxSlot (
    _In_ DEVICE_CONTEXT* DeviceContextPtr
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqQueueMessageAsync (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ ULONG MessageId,
    _In_reads_bytes_(BufferSize) VOID* BufferPtr,
    _In_ ULONG BufferSize
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqQueueMultiElementAsync (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ ULONG MessageId,
    _In_ VCHIQ_ELEMENT* ElementsPtr,
    _In_ ULONG Count
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqProcessBulkTransfer (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ WDFREQUEST Request,
    _In_ VCHIQ_QUEUE_BULK_TRANSFER* BulkTransferPtr,
    _In_ ULONG MsgDirection,
    _In_ MDL* BufferMdl,
    _In_ ULONG BufferSize
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqBulkTransfer (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ WDFREQUEST Request,
    _In_ ULONG MsgDirection,
    _In_ MDL* BufferMdl,
    _In_ ULONG BufferSize,
    _In_ ULONG ArmPortNumber,
    _In_ ULONG VchiqPortNumber
    );

_Requires_lock_held_(&VchiqFileContextPtr->PendingDataMsgMutex)
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqProcessPendingMsg (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr
    );

_Requires_lock_held_(&VchiqFileContextPtr->PendingVchiMsgMutex)
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqProcessPendingVchiMsg(
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqAddRefMsg (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ ULONG SlotNumber
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqReleaseMsg (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ ULONG SlotNumber
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqProcessNewRxMsg (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ VCHIQ_HEADER* RxMsg
    );

_Requires_lock_held_(&VchiqFileContextPtr->PendingDataMsgMutex)
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqAddPendingMsg (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ VCHIQ_HEADER* Msg,
    _In_ ULONG SlotNumber
    );

_Requires_lock_held_(&VchiqFileContextPtr->PendingDataMsgMutex)
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqRemovePendingMsg (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_opt_ WDFREQUEST WdfRequest
    );

_Requires_lock_held_(&VchiqFileContextPtr->PendingBulkMsgMutex[BulkType])
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqAddPendingBulkMsg (
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ VCHIQ_QUEUE_BULK_TRANSFER* BulkTransferPtr,
    _In_ MSG_BULK_TYPE BulkType
    );

_Requires_lock_held_(&VchiqFileContextPtr->PendingBulkMsgMutex[BulkType])
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqRemovePendingBulkMsg (
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _Inout_opt_ VCHIQ_COMPLETION_DATA* CompletionDataPtr,
    _In_ MSG_BULK_TYPE BulkType,
    _In_ ULONG RemoveAll,
    _Out_opt_ VCHIQ_BULK_MODE_T* BulkMode
    );

_Requires_lock_held_(&VchiqFileContextPtr->PendingVchiMsgMutex)
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqAddPendingVchiMsg (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ VCHIQ_HEADER* Msg,
    _In_ ULONG SlotNumber
    );

_Requires_lock_held_(&VchiqFileContextPtr->PendingVchiMsgMutex)
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqRemovePendingVchiMsg (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_opt_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID VchiqRecycleSlot (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VCHIQ_SLOT_ZERO* SlotZeroPtr,
    _In_ ULONG SlotNumber,
    _In_ BOOLEAN ReleaseSlot
    );

KSTART_ROUTINE VchiqTriggerThreadRoutine;

KSTART_ROUTINE VchiqRecycleThreadRoutine;

KSTART_ROUTINE VchiqSyncThreadRoutine;

KSTART_ROUTINE VchiqSyncReleaseThreadRoutine;

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS VchiqWaitForEvents (
    _In_ VOID* MainEventPtr,
    _In_ KEVENT* StopEventPtr,
    _In_opt_ LARGE_INTEGER* TimeoutPtr
    );

EXTERN_C_END