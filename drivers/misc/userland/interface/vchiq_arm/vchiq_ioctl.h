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

#ifndef VCHIQ_IOCTLS_H
#define VCHIQ_IOCTLS_H

#ifdef WIN32

#ifdef WIN32_KERN
#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>
#include <wdm.h>
#include <Ntstrsafe.h>
#else
#include <Windows.h>
#endif

#define FILE_DEVICE_VCHIQ          2835
#define VCHIQ_NAME "VCHIQ"
#define VCHIQ_NAME_W L"VCHIQ"
#define VCHIQ_SYMBOLIC_NAME "\\DosDevices\\" VCHIQ_NAME
#define VCHIQ_SYMBOLIC_NAME_W L"\\DosDevices\\" VCHIQ_NAME_W
#define VCHIQ_USERMODE_PATH "\\\\.\\" VCHIQ_NAME
#define VCHIQ_USERMODE_PATH_W L"\\\\.\\" VCHIQ_NAME_W

#ifdef _IO
#undef _IO
#endif
#define _IO(a, b) \
    CTL_CODE(FILE_DEVICE_VCHIQ, b, METHOD_BUFFERED , FILE_ANY_ACCESS)

#ifdef _IOW
#undef _IOW
#endif
#define _IOW(a, b, c) \
    CTL_CODE(FILE_DEVICE_VCHIQ, b, METHOD_OUT_DIRECT , FILE_ANY_ACCESS)

#ifdef _IOWR
#undef _IOR
#endif
#define _IOWR(a, b, c) \
    CTL_CODE(FILE_DEVICE_VCHIQ, b, METHOD_IN_DIRECT , FILE_ANY_ACCESS)

#else
#include <linux/ioctl.h>
#endif
#include "vchiq_if.h"

#define VCHIQ_IOC_MAGIC 0xc4
#define VCHIQ_INVALID_HANDLE (~0)

typedef struct {
   VCHIQ_SERVICE_PARAMS_T params;
   int is_open;
   int is_vchi;
   unsigned int handle;       /* OUT */
} VCHIQ_CREATE_SERVICE_T;

typedef struct {
   unsigned int handle;
   unsigned int count;
   const VCHIQ_ELEMENT_T *elements;
   // For windows we require additional memory
   // handle so driver can access user mode pointer
#ifdef WIN32
   void *driver_element_handle;
#endif
} VCHIQ_QUEUE_MESSAGE_T;

typedef struct {
   unsigned int handle;
   void *data;
   unsigned int size;
   void *userdata;
   VCHIQ_BULK_MODE_T mode;
   // For windows we require additional memory
   // handle so driver can access user mode pointer
#ifdef WIN32
   void *driver_buffer_handle;
#endif
} VCHIQ_QUEUE_BULK_TRANSFER_T;

typedef struct {
   VCHIQ_REASON_T reason;
   VCHIQ_HEADER_T *header;
   void *service_userdata;
   void *bulk_userdata;
   // For windows we require additional memory
   // handle so driver can access user mode pointer
#ifdef WIN32
   void *driver_buffer_handle;
#endif
} VCHIQ_COMPLETION_DATA_T;

typedef struct {
   unsigned int count;
   VCHIQ_COMPLETION_DATA_T *buf;
   unsigned int msgbufsize;
   unsigned int msgbufcount; /* IN/OUT */
   void **msgbufs;
   // For windows we require additional memory
   // handle so driver can access user mode pointer
#ifdef WIN32
   void *driver_completion_handle;
#endif
} VCHIQ_AWAIT_COMPLETION_T;

typedef struct {
   unsigned int handle;
   int blocking;
   unsigned int bufsize;
   void *buf;
#ifdef WIN32
   void *driver_buffer_handle;
#endif
} VCHIQ_DEQUEUE_MESSAGE_T;

typedef struct {
   unsigned int config_size;
   VCHIQ_CONFIG_T *pconfig;
#ifdef WIN32
   void *driver_config_handle;
#endif
} VCHIQ_GET_CONFIG_T;

typedef struct {
   unsigned int handle;
   VCHIQ_SERVICE_OPTION_T option;
   int value;
} VCHIQ_SET_SERVICE_OPTION_T;

typedef struct {
   void     *virt_addr;
   size_t    num_bytes;
} VCHIQ_DUMP_MEM_T;

#define VCHIQ_IOC_CONNECT              _IO(VCHIQ_IOC_MAGIC,   0)
#define VCHIQ_IOC_SHUTDOWN             _IO(VCHIQ_IOC_MAGIC,   1)
#define VCHIQ_IOC_CREATE_SERVICE       _IOWR(VCHIQ_IOC_MAGIC, 2, VCHIQ_CREATE_SERVICE_T)
#define VCHIQ_IOC_REMOVE_SERVICE       _IO(VCHIQ_IOC_MAGIC,   3)
#define VCHIQ_IOC_QUEUE_MESSAGE        _IOW(VCHIQ_IOC_MAGIC,  4, VCHIQ_QUEUE_MESSAGE_T)
#define VCHIQ_IOC_QUEUE_BULK_TRANSMIT  _IOWR(VCHIQ_IOC_MAGIC, 5, VCHIQ_QUEUE_BULK_TRANSFER_T)
#ifdef WIN32
// Used buffer memory for Windows for better memory alignment
#define VCHIQ_IOC_QUEUE_BULK_RECEIVE   _IO(VCHIQ_IOC_MAGIC, 6)
#else
#define VCHIQ_IOC_QUEUE_BULK_RECEIVE   _IOWR(VCHIQ_IOC_MAGIC, 6, VCHIQ_QUEUE_BULK_TRANSFER_T)
#endif
#define VCHIQ_IOC_AWAIT_COMPLETION     _IOWR(VCHIQ_IOC_MAGIC, 7, VCHIQ_AWAIT_COMPLETION_T)
#define VCHIQ_IOC_DEQUEUE_MESSAGE      _IOWR(VCHIQ_IOC_MAGIC, 8, VCHIQ_DEQUEUE_MESSAGE_T)
#define VCHIQ_IOC_GET_CLIENT_ID        _IO(VCHIQ_IOC_MAGIC,   9)
#define VCHIQ_IOC_GET_CONFIG           _IOWR(VCHIQ_IOC_MAGIC, 10, VCHIQ_GET_CONFIG_T)
#define VCHIQ_IOC_CLOSE_SERVICE        _IO(VCHIQ_IOC_MAGIC,   11)
#define VCHIQ_IOC_USE_SERVICE          _IO(VCHIQ_IOC_MAGIC,   12)
#define VCHIQ_IOC_RELEASE_SERVICE      _IO(VCHIQ_IOC_MAGIC,   13)
#define VCHIQ_IOC_SET_SERVICE_OPTION   _IOW(VCHIQ_IOC_MAGIC,  14, VCHIQ_SET_SERVICE_OPTION_T)
#define VCHIQ_IOC_DUMP_PHYS_MEM        _IOW(VCHIQ_IOC_MAGIC,  15, VCHIQ_DUMP_MEM_T)
#define VCHIQ_IOC_LIB_VERSION          _IO(VCHIQ_IOC_MAGIC,   16)
#define VCHIQ_IOC_CLOSE_DELIVERED      _IO(VCHIQ_IOC_MAGIC,   17)
#define VCHIQ_IOC_MAX                  17

#endif
