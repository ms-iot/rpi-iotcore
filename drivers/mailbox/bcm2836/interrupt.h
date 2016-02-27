//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    interrupt.h
//
// Abstract:
//
//    This file contains the interrupt related definitions.
//

#pragma once

EXTERN_C_START

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS RpiqEnableInterrupts (
    _In_ DEVICE_CONTEXT* DeviceContextPtr
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS RpiqDisableInterrupts (
    _In_ DEVICE_CONTEXT* DeviceContextPtr
    );

EVT_WDF_INTERRUPT_ISR RpiqMailboxIsr;
EVT_WDF_INTERRUPT_DPC RpiqMailboxDpc;

EXTERN_C_END