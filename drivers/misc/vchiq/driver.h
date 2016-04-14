//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     driver.h
//
// Abstract:
//
//      VCHIQ main driver entry headers
//

#pragma once

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_UNLOAD VchiqOnDriverUnload;
EVT_WDF_DRIVER_DEVICE_ADD VchiqOnDeviceAdd;

EXTERN_C_END