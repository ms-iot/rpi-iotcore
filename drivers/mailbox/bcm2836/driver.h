//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    driver.h
//
// Abstract:
//
//  
//

#pragma once

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_UNLOAD RpiqOnDriverUnload;
EVT_WDF_DRIVER_DEVICE_ADD RpiqOnDeviceAdd;

EXTERN_C_END
