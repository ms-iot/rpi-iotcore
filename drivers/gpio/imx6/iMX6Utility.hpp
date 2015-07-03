#ifndef _IMX6UTILITY_HPP_
#define _IMX6UTILITY_HPP_ 1

//
// Freescale i.MX6 GPIO Client Driver
//
// Copyright(c) Microsoft Corporation
//
// All rights reserved.
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this software and associated documentation files(the ""Software""),
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and / or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Module Name:
//
//  iMX6Utility.hpp
//
// Abstract:
//
//    This file contains helpers used by iMX6Gpio.
//
// Environment:
//
//    Kernel mode only.
//

//
// Macros to be used for proper PAGED/NON-PAGED code placement
//

#define IMX_NONPAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    //__pragma(code_seg(.text))

#define IMX_NONPAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define IMX_PAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("PAGE"))

#define IMX_PAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define IMX_INIT_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("INIT"))

#define IMX_INIT_SEGMENT_END \
    __pragma(code_seg(pop))

//
// We have some non-paged functions that supposed to be called on low IRQL.
// The following macro defines unified assert to be put at the beginning of
// such functions.
//
// NOTE: We can't use standard PAGED_CODE macro as it requires function to be
// placed in paged segment during compilation.
//
#define IMX_ASSERT_MAX_IRQL(Irql) NT_ASSERT(KeGetCurrentIrql() <= (Irql))
#define IMX_ASSERT_LOW_IRQL() IMX_ASSERT_MAX_IRQL(APC_LEVEL)

//
// Default memory allocation and object construction for C++ modules
//

inline void* __cdecl operator new (
    size_t Size,
    POOL_TYPE PoolType,
    ULONG Tag
    ) throw ()
{
    if (!Size) Size = 1;
    return ExAllocatePoolWithTag(PoolType, Size, Tag);
} // operator new (size_t, POOL_TYPE, ULONG)

inline void __cdecl operator delete (
    void* Ptr,
    POOL_TYPE /*PoolType*/,
    ULONG Tag
    ) throw ()
{
    if (Ptr) ExFreePoolWithTag(Ptr, Tag);
} // operator delete (void*, POOL_TYPE, ULONG)

inline void __cdecl operator delete (
    void* Ptr
    ) throw ()
{
    if (Ptr) ExFreePool(Ptr);
} // operator delete (void*)

inline void* __cdecl operator new[] (
    size_t Size,
    POOL_TYPE PoolType,
    ULONG Tag
    ) throw ()
{
    if (!Size) Size = 1;
    return ExAllocatePoolWithTag(PoolType, Size, Tag);
} // operator new[] (size_t, POOL_TYPE, ULONG)

inline void __cdecl operator delete[] (
    void* Ptr,
    POOL_TYPE /*PoolType*/,
    ULONG Tag
    ) throw ()
{
    if (Ptr) ExFreePoolWithTag(Ptr, ULONG(Tag));
} // operator delete[] (void*, POOL_TYPE, ULONG)

inline void __cdecl operator delete[] (void* Ptr) throw ()
{
    if (Ptr) ExFreePool(Ptr);
} // operator delete[] (void*)

inline void* __cdecl operator new (size_t, void* Ptr) throw ()
{
    return Ptr;
} // operator new (size_t, void*)

inline void __cdecl operator delete (void*, void*) throw ()
{} // void operator delete (void*, void*)

inline void* __cdecl operator new[] (size_t, void* Ptr) throw ()
{
    return Ptr;
} // operator new[] (size_t, void*)

inline void __cdecl operator delete[] (void*, void*) throw ()
{} // void operator delete[] (void*, void*)

inline void __cdecl operator delete (void*, unsigned int) throw ()
{} // void operator delete (void*, unsigned int)

#endif // _IMX6UTILITY_HPP_
