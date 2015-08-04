/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    driver.h

Abstract:

    This module contains the function definitions for
    the WDF driver.

Environment:

    kernel-mode only

Revision History:

--*/

#ifndef _DRIVER_H_
#define _DRIVER_H_

//
// Macros to be used for proper PAGED/NON-PAGED code placement
//

#define BCM_I2C_NONPAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    //__pragma(code_seg(.text))

#define BCM_I2C_NONPAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define BCM_I2C_PAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("PAGE"))

#define BCM_I2C_PAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define BCM_I2C_INIT_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("INIT"))

#define BCM_I2C_INIT_SEGMENT_END \
    __pragma(code_seg(pop))

#define BCM_I2C_ASSERT_MAX_IRQL(Irql) NT_ASSERT(KeGetCurrentIrql() <= (Irql))

enum : ULONG {
    BCM_I2C_POOL_TAG = 'IMCB'
};

EVT_WDF_DRIVER_DEVICE_ADD OnDeviceAdd;

EVT_WDF_DRIVER_UNLOAD OnDriverUnload;
extern "C" DRIVER_INITIALIZE DriverEntry;

#endif // _DRIVER_H_
