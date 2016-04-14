/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Copyright © 2015 Microsoft Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the “Software”), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/*=============================================================================
VideoCore OS Abstraction Layer - platform-specific types and defines
=============================================================================*/

#ifndef VCOS_PLATFORM_TYPES_H
#define VCOS_PLATFORM_TYPES_H

#include "interface/vcos/vcos_inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __func__
#define __func__ __FUNCTION__
#endif

#ifndef __inline__
#define __inline__ __inline
#endif

#if defined(WIN32DLL_EXPORTS) && defined(VCOS_PROVIDER)
#define WIN32DLL_VCOS_API __declspec(dllexport)
#else
#define WIN32DLL_VCOS_API __declspec(dllimport)
#endif

#define VCOSPRE_ WIN32DLL_VCOS_API
#define VCOSPOST_ 

#if defined(__GNUC__) && (( __GNUC__ > 2 ) || (( __GNUC__ == 2 ) && ( __GNUC_MINOR__ >= 3 )))
#define VCOS_FORMAT_ATTR_(ARCHETYPE, STRING_INDEX, FIRST_TO_CHECK)  __attribute__ ((format (ARCHETYPE, STRING_INDEX, FIRST_TO_CHECK)))
#else
#define VCOS_FORMAT_ATTR_(ARCHETYPE, STRING_INDEX, FIRST_TO_CHECK)
#endif

#if defined(__linux__) && !defined(NDEBUG) && defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
   #define VCOS_BKPT ({ __asm volatile ("int3":::"memory"); })
#endif
/*#define VCOS_BKPT vcos_abort() */

#define VCOS_ASSERT_LOGGING         1
#define VCOS_ASSERT_LOGGING_DISABLE 0

VCOSPRE_ void
vcos_pthreads_logging_assert(const char *file, const char *func, unsigned int line, const char *fmt, ...);

#define VCOS_ASSERT_MSG(...) ((VCOS_ASSERT_LOGGING && !VCOS_ASSERT_LOGGING_DISABLE) ? vcos_pthreads_logging_assert(__FILE__, __func__, __LINE__, __VA_ARGS__) : (void)0)

#define VCOS_INLINE_BODIES
#define VCOS_INLINE_DECL extern __inline__
#define VCOS_INLINE_IMPL static __inline__

#ifdef __cplusplus
}
#endif

#endif
