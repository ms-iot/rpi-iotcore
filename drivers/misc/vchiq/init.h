//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     init.h
//
// Abstract:
//
//     This header contain early initialization related definition
//

#pragma once

EXTERN_C_START

EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED VchiqInitOperation;
DRIVER_NOTIFICATION_CALLBACK_ROUTINE VchiqInterfaceCallback;

EXTERN_C_END
