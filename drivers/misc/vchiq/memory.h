//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     memory.h
//
// Abstract:
//
//     This file contains memory related definitions.
//

#pragma once

EXTERN_C_START

#define MEMORY_SIZE_1_G              0x40000000

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqAllocPhyContiguous (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ ULONG BufferSize,
    _Outptr_ VOID** BufferPPtr
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS VchiqAllocateCommonBuffer (
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ ULONG BufferSize,
    _Outptr_ VOID** BufferPPtr,
    _Out_ PHYSICAL_ADDRESS* PhyAddressPtr
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS VchiqFreeCommonBuffer (
    _In_ VCHIQ_FILE_CONTEXT* VchiqFileContextPtr,
    _In_ ULONG BufferSize,
    _In_ PHYSICAL_ADDRESS PhyAddress,
    _In_ VOID* BufferPtr
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS VchiqFreePhyContiguous (
    _In_ DEVICE_CONTEXT* DeviceContextPtr,
    _In_ VOID** BufferPPtr
    );

EXTERN_C_END
