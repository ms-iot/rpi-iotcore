//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    slotmgmt.h
//
// Abstract:
//
//    This file contains the definitions for commont slot management
//    definition.
//

#pragma once

EXTERN_C_START

typedef struct _SLOT_INFO {
    volatile LONG RefCount;
    BOOLEAN SlotInUse;
}SLOT_INFO, *PSLOT_INFO;

typedef struct _VCHIQ_PENDING_MSG {
    VCHIQ_HEADER* Msg;
    ULONG SlotNumber;
    WDFMEMORY WdfMemory;
    LIST_ENTRY ListEntry;
}VCHIQ_PENDING_MSG, *PVCHIQ_PENDING_MSG;

typedef struct _VCHIQ_PENDING_BULK_MSG {
    VOID* BulkUserData;
    WDFMEMORY WdfMemory;
    LIST_ENTRY ListEntry;
    VCHIQ_BULK_MODE_T Mode;
}VCHIQ_PENDING_BULK_MSG, *PVCHIQ_PENDING_BULK_MSG;

EXTERN_C_END