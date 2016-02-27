//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    init.h
//
// Abstract:
//
//    This header contain early initialization related definition
//

#pragma once

EXTERN_C_START

EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED RpiqInitOperation;

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS RpiSetDeviceMacAddress(
    _In_ DEVICE_CONTEXT* DeviceContextPtr
    );

EXTERN_C_END
