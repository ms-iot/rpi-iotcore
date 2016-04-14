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
VideoCore OS Abstraction Layer - named semaphores
=============================================================================*/

#ifndef VCOS_NAMED_SEMAPHORE_H
#define VCOS_NAMED_SEMAPHORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos_platform.h"

/**
 * \file
 *
 * Create a named semaphore, or open an existing one by name.
 *
 */

/** 
  * \brief Create a named semaphore.
  *
  * Semaphores are not re-entrant.
  *
  * @param sem   Pointer to memory to be initialized
  * @param name  A name for this semaphore.
  * @param count The initial count for the semaphore.
  *
  * @return VCOS_SUCCESS if the semaphore was created.
  * 
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_named_semaphore_create(VCOS_NAMED_SEMAPHORE_T *sem, const char *name, VCOS_UNSIGNED count);

/**
  * \brief Wait on a named semaphore.
  *
  * There is no timeout option on a semaphore, as adding this will slow down
  * implementations on some platforms. If you need that kind of behaviour, use
  * an event group.
  *
  * This always returns VCOS_SUCCESS and so should really be a void function. However
  * too many lines of code would need to be changed in non-trivial ways, so for now
  * it is non-void.
  *
  * @param sem Semaphore to wait on
  * @return VCOS_SUCCESS - semaphore was taken.
  *
  */
VCOS_INLINE_DECL
void vcos_named_semaphore_wait(VCOS_NAMED_SEMAPHORE_T *sem);

/**
  * \brief Try to wait for a semaphore.
  *
  * Try to obtain the semaphore. If it is already taken, return VCOS_TIMEOUT.
  * @param sem Semaphore to wait on
  * @return VCOS_SUCCESS - semaphore was taken.
  *         VCOS_EAGAIN - could not take semaphore
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_named_semaphore_trywait(VCOS_NAMED_SEMAPHORE_T *sem);

/**
  * \brief Post a semaphore.
  *
  * @param sem Semaphore to wait on
  */
VCOS_INLINE_DECL
void vcos_named_semaphore_post(VCOS_NAMED_SEMAPHORE_T *sem);

/**
  * \brief Delete a semaphore, releasing any resources consumed by it.
  *
  * @param sem Semaphore to wait on
  */
VCOS_INLINE_DECL
void vcos_named_semaphore_delete(VCOS_NAMED_SEMAPHORE_T *sem);


#ifdef __cplusplus
}
#endif
#endif

