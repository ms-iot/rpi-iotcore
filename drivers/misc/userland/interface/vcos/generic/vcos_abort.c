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

#include "interface/vcos/vcos.h"
#ifdef __VIDEOCORE__
#include "host_support/include/vc_debug_sym.h"
#include "vcfw/vclib/vclib.h"
#endif
#include <stdlib.h>


int vcos_verify_bkpts = 0;
#ifdef __VIDEOCORE__
VC_DEBUG_VAR(vcos_verify_bkpts);
#endif

int vcos_verify_bkpts_enabled(void)
{
   return vcos_verify_bkpts;
}

int vcos_verify_bkpts_enable(int enable)
{
   int old = vcos_verify_bkpts;
   vcos_verify_bkpts = enable;
   return old;
}

/**
  * Call the fatal error handler.
  */
void vcos_abort(void)
{
   VCOS_ALERT("vcos_abort: Halting");

#ifdef WIN32
   __debugbreak();
#endif

#ifdef __VIDEOCORE__
   _bkpt();
#endif

#if defined(VCOS_HAVE_BACKTRACE) && !defined(NDEBUG)
   vcos_backtrace_self();
#endif

#ifdef __VIDEOCORE__
   /* Flush the cache to help with postmortem RAM-dump debugging */
   vclib_cache_flush();
#endif

#ifdef PLATFORM_RASPBERRYPI
   extern void pattern(int);
   while(1)
      pattern(8);
#endif

   /* Insert chosen fatal error handler here */
#if defined __VIDEOCORE__ && !defined(NDEBUG)
   while(1); /* allow us to attach a debugger after the fact and see where we came from. */
#else
#ifndef WIN32_KERN
   abort(); /* on vc this ends up in _exit_halt which doesn't give us any stack backtrace */
#endif
#endif
}
