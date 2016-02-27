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

namespace _DETAILS {

// Disables template argument deduction from Forward helper
template<class T>
struct IDENTITY {
    // Map T to type unchanged
    typedef T TYPE;
};

template<class T>
inline T&& Forward (typename IDENTITY<T>::TYPE& arg) throw()
{
    // Forward arg, given explicitly specified Type parameter
    return (T&&)arg;
}

} // namespace _DETAILS

template <typename Fn>
struct _FINALLY : public Fn {
    __forceinline _FINALLY (Fn&& Func) : Fn(_DETAILS::Forward<Fn>(Func)) {}
    __forceinline _FINALLY (const _FINALLY&); // generate link error if copy constructor is called
    __forceinline ~_FINALLY () { this->operator()(); }
};

template <typename Fn>
__forceinline _FINALLY<Fn> Finally (Fn&& Func)
{
    return {_DETAILS::Forward<Fn>(Func)};
}

EVT_WDF_DRIVER_DEVICE_ADD OnDeviceAdd;

EVT_WDF_DRIVER_UNLOAD OnDriverUnload;
extern "C" DRIVER_INITIALIZE DriverEntry;

#endif // _DRIVER_H_
