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

/*#define VCOS_INLINE_BODIES */
#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_msgqueue.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

/* Cygwin doesn't always have prctl.h and it doesn't have PR_SET_NAME */
#if defined( __linux__ )
# if !defined(HAVE_PRCTL)
#  define HAVE_PRCTL
# endif
#include <sys/prctl.h>
#endif

#ifdef HAVE_CMAKE_CONFIG
#include "cmake_config.h"
#endif

#ifndef VCOS_DEFAULT_STACK_SIZE
#define VCOS_DEFAULT_STACK_SIZE 4096
#endif

static int vcos_argc;
static const char **vcos_argv;

typedef void (*LEGACY_ENTRY_FN_T)(int, void *);

static VCOS_THREAD_ATTR_T default_attrs = {
   .ta_stacksz = VCOS_DEFAULT_STACK_SIZE,
};

#ifdef WIN32_KERN
// DriverEntry implementation for kernel mode
DRIVER_INITIALIZE DriverEntry;

NTSTATUS DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    return STATUS_SUCCESS;
}
#endif

/** Singleton global lock used for vcos_global_lock/unlock(). */
//static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#ifdef WIN32_KERN
    static FAST_MUTEX lock;
    static BOOLEAN global_lock_init = FALSE;
#else
    static CRITICAL_SECTION lock;
    static BOOL global_lock_init = FALSE;
#endif
/* Create a per-thread key for faking up vcos access
 * on non-vcos threads.
 */
DWORD _vcos_thread_current_key;

static VCOS_UNSIGNED _vcos_thread_current_key_created = 0;
static VCOS_ONCE_T current_thread_key_once = RTL_RUN_ONCE_INIT;  /* init just once */

#ifdef WIN32_KERN

#else
// Minimize changes to the origianl userland implementation so disabling
// deprecated function warning
#pragma warning(disable : 4996)
#endif

#ifdef WIN32_KERN
ULONG InitHandleFunction(
    PRTL_RUN_ONCE InitOnce,
    PVOID Parameter,
    PVOID *lpContext)
#else
BOOL CALLBACK InitHandleFunction(
    PINIT_ONCE InitOnce,
    PVOID Parameter,
    PVOID *lpContext)
#endif
{
#pragma warning(disable : 4055)
    VCOS_THREAD_ENTRY_FN_T function = (VCOS_THREAD_ENTRY_FN_T)Parameter;

    UNREFERENCED_PARAMETER(InitOnce);

    if (function == NULL) {
        return 0;
    }

    function(lpContext);

#ifdef WIN32_KERN
    return 1;
#else
    return TRUE;
#endif
}

static void vcos_thread_cleanup(VCOS_THREAD_T *thread)
{
   vcos_semaphore_delete(&thread->suspend);
   if (thread->task_timer_created)
   {
      vcos_timer_delete(&thread->task_timer);
   }
}

static void vcos_dummy_thread_cleanup(void *cxt)
{
   VCOS_THREAD_T *thread = cxt;
   if (thread->dummy)
   {
      int i;
      /* call termination functions */
      for (i=0; thread->at_exit[i].pfn != NULL; i++)
      {
         thread->at_exit[i].pfn(thread->at_exit[i].cxt);
      }
      vcos_thread_cleanup(thread);
      vcos_free(thread);
   }
}

static void current_thread_key_init(void)
{
#ifdef WIN32_KERN 
    // TODO : Implement kernel version
    _vcos_thread_current_key_created = 1;
#else
    _vcos_thread_current_key = TlsAlloc();
    _vcos_thread_current_key_created = 1;
#endif
}


/* A VCOS wrapper for the thread which called vcos_init. */
static VCOS_THREAD_T vcos_thread_main;

#ifdef WIN32_KERN 
static void vcos_thread_entry(void *arg)
{
   int i;
   const void *ret;
   VCOS_THREAD_T *thread = (VCOS_THREAD_T *)arg;

   vcos_assert(thread != NULL);
   thread->dummy = 0;

   // TODO : Implement kernel version
   // TlsSetValue(_vcos_thread_current_key, arg);

   if (thread->legacy)
   {
      LEGACY_ENTRY_FN_T fn = (LEGACY_ENTRY_FN_T)thread->entry;
      (*fn)(0, thread->arg);
      ret = 0;
   }
   else
   {
      ret = (*thread->entry)(thread->arg);
   }

   /* call termination functions */
   for (i=0; thread->at_exit[i].pfn != NULL; i++)
   {
      thread->at_exit[i].pfn(thread->at_exit[i].cxt);
   }

   return;
}
#else
static void *vcos_thread_entry(void *arg)
{
    int i;
    void *ret;
    VCOS_THREAD_T *thread = (VCOS_THREAD_T *)arg;

    vcos_assert(thread != NULL);
    thread->dummy = 0;

    TlsSetValue(_vcos_thread_current_key, arg);

    if (thread->legacy)
    {
        LEGACY_ENTRY_FN_T fn = (LEGACY_ENTRY_FN_T)thread->entry;
        (*fn)(0, thread->arg);
        ret = 0;
    } else
    {
        ret = (*thread->entry)(thread->arg);
    }

    /* call termination functions */
    for (i = 0; thread->at_exit[i].pfn != NULL; i++)
    {
        thread->at_exit[i].pfn(thread->at_exit[i].cxt);
    }

    return ret;
}
#endif

static void _task_timer_expiration_routine(void *cxt)
{
   VCOS_THREAD_T *thread = (VCOS_THREAD_T *)cxt;

   vcos_assert(thread->orig_task_timer_expiration_routine);
   thread->orig_task_timer_expiration_routine(thread->orig_task_timer_context);
   thread->orig_task_timer_expiration_routine = NULL;
}

VCOS_STATUS_T vcos_thread_create(VCOS_THREAD_T *thread,
                                 const char *name,
                                 VCOS_THREAD_ATTR_T *attrs,
                                 VCOS_THREAD_ENTRY_FN_T entry,
                                 void *arg)
{
    VCOS_STATUS_T st;
    const VCOS_THREAD_ATTR_T *local_attrs = attrs ? attrs : &default_attrs;

    vcos_assert(thread);
    memset(thread, 0, sizeof(VCOS_THREAD_T));

    st = vcos_semaphore_create(&thread->suspend, NULL, 0);
    if (st != VCOS_SUCCESS)
    {
        return st;
    }
    vcos_assert(local_attrs->ta_stackaddr == 0);
    thread->entry = entry;
    thread->arg = arg;
    thread->legacy = local_attrs->legacy;

#ifdef WIN32_KERN
    strncpy_s(thread->name, sizeof(thread->name), name, sizeof(thread->name));
#else
    strncpy(thread->name, name, sizeof(thread->name));
#endif

    thread->name[sizeof(thread->name) - 1] = '\0';
    memset(thread->at_exit, 0, sizeof(thread->at_exit));

#ifdef WIN32_KERN
    OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes(
        &objectAttributes,
        NULL,
        OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    NTSTATUS status = PsCreateSystemThread(
        &thread->thread,
        THREAD_ALL_ACCESS,
        &objectAttributes,
        NULL,
        NULL,
        vcos_thread_entry,
        thread);
    if (!NT_SUCCESS(status))
    {
        vcos_semaphore_delete(&thread->suspend);
        st = VCOS_ENOMEM;
    }
#else
    thread->thread = CreateThread(NULL, local_attrs->ta_stacksz, (LPTHREAD_START_ROUTINE)vcos_thread_entry, thread, 0, NULL);
    if (thread->thread == NULL)
    {
        vcos_semaphore_delete(&thread->suspend);
        st = VCOS_ENOMEM;
    }
#endif

    return st;
}

void vcos_thread_join(VCOS_THREAD_T *thread,
                             void **pData)
{
#ifdef WIN32_KERN 
    UNREFERENCED_PARAMETER(pData);

    DWORD result = KeWaitForSingleObject(thread->thread, Executive, KernelMode, FALSE, NULL);
    if (result != STATUS_SUCCESS) {
        // Signal error?
    }
#else
    UNREFERENCED_PARAMETER(pData);
    DWORD result = WaitForSingleObject(thread->thread, INFINITE);
    if (result != WAIT_OBJECT_0) {
        // Signal error?
    }
#endif
}

VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_thread_create_classic(VCOS_THREAD_T *thread,
                                                            const char *name,
                                                            void *(*entry)(void *arg),
                                                            void *arg,
                                                            void *stack,
                                                            VCOS_UNSIGNED stacksz,
                                                            VCOS_UNSIGNED priaff,
                                                            VCOS_UNSIGNED timeslice,
                                                            VCOS_UNSIGNED autostart)
{
   VCOS_THREAD_ATTR_T attrs;
   vcos_thread_attr_init(&attrs);
   vcos_thread_attr_setstacksize(&attrs, stacksz);
   vcos_thread_attr_setpriority(&attrs, priaff & ~_VCOS_AFFINITY_MASK);
   vcos_thread_attr_setaffinity(&attrs, priaff & _VCOS_AFFINITY_MASK);
   (void)timeslice;
   (void)autostart;

#ifdef VCOS_CAN_SET_STACK_ADDR
   {
      vcos_thread_attr_setstack(&attrs, stack, stacksz);
   }
#endif

   return vcos_thread_create(thread, name, &attrs, entry, arg);
}

uint64_t vcos_getmicrosecs64_internal(void)
{
#ifdef WIN32_KERN
    LARGE_INTEGER time;

    __pragma(prefast(suppress:25003, "Nonconst local within KeQueryTickCount"))
    KeQueryTickCount(&time);

    return time.QuadPart;
#else
    // QuerryPerformanceCounter if require beter accuracy
    return GetTickCount64();
#endif
}

// This is android related flag is preserve for backward compatibility
int vcos_use_android_log = 0;
int vcos_log_to_file = 0;

static FILE * log_fhandle = NULL;

void vcos_vlog_default_impl(const VCOS_LOG_CAT_T *cat, VCOS_LOG_LEVEL_T _level, const char *fmt, va_list args)
{
    (void)_level;
#ifdef WIN32_KERN
    UNREFERENCED_PARAMETER(cat);

    if(NULL != log_fhandle)
    {
        DbgPrint(fmt, args);
    }
#else
    if (NULL != log_fhandle)
    {
        if (cat->flags.want_prefix)
            fprintf(log_fhandle, "%s: ", cat->name);
        vfprintf(log_fhandle, fmt, args);
        fputs("\n", log_fhandle);
        fflush(log_fhandle);
    }
#endif
}

void _vcos_log_platform_init(void)
{
#ifdef WIN32_KERN
    // TODO : Implement logging
#else
    if (vcos_log_to_file)
    {
        char log_fname[100];
        snprintf(log_fname, 100, "/var/log/vcos_log%u.txt", vcos_process_id_current());
        log_fhandle = fopen(log_fname, "w");
    } else
        log_fhandle = stderr;
#endif
}

/* Flags for init/deinit components */
enum
{
   VCOS_INIT_NAMED_SEM   = (1 << 0),
   VCOS_INIT_PRINTF_LOCK = (1 << 1),
   VCOS_INIT_MAIN_SEM    = (1 << 2),
   VCOS_INIT_MSGQ        = (1 << 3),
   VCOS_INIT_ALL         = 0xffffffff
};

static void vcos_term(uint32_t flags)
{
   if (flags & VCOS_INIT_MSGQ)
      vcos_msgq_deinit();

   if (flags & VCOS_INIT_MAIN_SEM)
      vcos_semaphore_delete(&vcos_thread_main.suspend);

   if (flags & VCOS_INIT_NAMED_SEM)
      _vcos_named_semaphore_deinit();
}

VCOS_STATUS_T vcos_platform_init(void)
{
   VCOS_STATUS_T st;
   uint32_t flags = 0;

   st = _vcos_named_semaphore_init();
   if (!vcos_verify(st == VCOS_SUCCESS))
      goto end;

   flags |= VCOS_INIT_NAMED_SEM;

   st = vcos_once(&current_thread_key_once, current_thread_key_init);
   if (!vcos_verify(st == VCOS_SUCCESS))
      goto end;

   /* Initialise a VCOS wrapper for the thread which called vcos_init. */
   st = vcos_semaphore_create(&vcos_thread_main.suspend, NULL, 0);
   if (!vcos_verify(st == VCOS_SUCCESS))
      goto end;

   flags |= VCOS_INIT_MAIN_SEM;

#ifdef WIN32_KERN 
   vcos_thread_main.thread = PsGetCurrentThreadId();

   // TODO Implement thread context for kernel mode
#else
   int pst;

   vcos_thread_main.thread = GetCurrentThread();

   // For windows zero return value is failure
   pst = TlsSetValue(_vcos_thread_current_key, &vcos_thread_main);
   if (!vcos_verify(pst != 0))
   {
       st = VCOS_EINVAL;
       goto end;
   }
#endif

   st = vcos_msgq_init();
   if (!vcos_verify(st == VCOS_SUCCESS))
      goto end;

   flags |= VCOS_INIT_MSGQ;

   vcos_logging_init();

end:
   if (st != VCOS_SUCCESS)
      vcos_term(flags);

   return st;
}

void vcos_platform_deinit(void)
{
   vcos_term((uint32_t)VCOS_INIT_ALL);
}

#ifdef WIN32_KERN 
// TODO : Figure out proper SAL notation for ExAcquireFastMutex and
//        ExReleaseFastMutex
#pragma warning(push)
#pragma warning(disable : 28167)
#endif

_Acquires_lock_(lock)
void vcos_global_lock(void)
{
#ifdef WIN32_KERN 
    if (global_lock_init == FALSE) {
        ExInitializeFastMutex(&lock);
        global_lock_init = TRUE;
    }
    ExAcquireFastMutex(&lock);
#else
    if (global_lock_init == FALSE) {
        InitializeCriticalSection(&lock);
        global_lock_init = TRUE;
    }
    EnterCriticalSection(&lock);
#endif
}

_Releases_lock_(lock)
void vcos_global_unlock(void)
{
#ifdef WIN32_KERN
    ExReleaseFastMutex(&lock);
#else
    if (global_lock_init == FALSE) {
        return;
    }
    LeaveCriticalSection(&lock);
#endif
}

#ifdef WIN32_KERN 
#pragma warning(pop)
#endif

void vcos_thread_exit(void *arg)
{
   VCOS_THREAD_T *thread = vcos_thread_current();

   if ( thread && thread->dummy )
   {
      vcos_free ( (void*) thread );
      thread = NULL;
   }

   // Do nothing
   UNREFERENCED_PARAMETER(arg);
}


void vcos_thread_attr_init(VCOS_THREAD_ATTR_T *attrs)
{
   *attrs = default_attrs;
}

VCOS_STATUS_T vcos_pthreads_map_error(int error)
{
   switch (error)
   {
   case ENOMEM:
      return VCOS_ENOMEM;
   case ENXIO:
      return VCOS_ENXIO;
   case EAGAIN:
      return VCOS_EAGAIN;
   case ENOSPC:
      return VCOS_ENOSPC;
   default:
      return VCOS_EINVAL;
   }
}

VCOS_STATUS_T vcos_pthreads_map_errno(void)
{
#ifdef WIN32_KERN
    // TODO : Expand error return for kernel mode
    return vcos_pthreads_map_error(VCOS_EINVAL);
#else
    return vcos_pthreads_map_error(errno);
#endif
}

void _vcos_task_timer_set(void (*pfn)(void*), void *cxt, VCOS_UNSIGNED ms)
{
   VCOS_THREAD_T *thread = vcos_thread_current();

   if (thread == NULL)
      return;

   vcos_assert(thread->orig_task_timer_expiration_routine == NULL);

   if (!thread->task_timer_created)
   {
      VCOS_STATUS_T st = vcos_timer_create(&thread->task_timer, NULL,
                                _task_timer_expiration_routine, thread);
      (void)st;
      vcos_assert(st == VCOS_SUCCESS);
      thread->task_timer_created = 1;
   }

   thread->orig_task_timer_expiration_routine = pfn;
   thread->orig_task_timer_context = cxt;

   vcos_timer_set(&thread->task_timer, ms);
}

void _vcos_task_timer_cancel(void)
{
   VCOS_THREAD_T *thread = vcos_thread_current();

   if (thread == NULL || !thread->task_timer_created)
     return;

   vcos_timer_cancel(&thread->task_timer);
   thread->orig_task_timer_expiration_routine = NULL;
}

int vcos_vsnprintf( char *buf, size_t buflen, const char *fmt, va_list ap )
{
#ifdef WIN32_KERN
    return RtlStringCbVPrintfA( buf, buflen, fmt, ap );
#else
    return vsnprintf(buf, buflen, fmt, ap);
#endif
}

int vcos_snprintf(char *buf, size_t buflen, const char *fmt, ...)
{
   int ret;
   va_list ap;
   va_start(ap,fmt);
#ifdef WIN32_KERN
   ret = RtlStringCbVPrintfA(buf, buflen, fmt, ap);
#else
   ret = vsnprintf(buf, buflen, fmt, ap);
#endif
   va_end(ap);
   return ret;
}

int vcos_have_rtos(void)
{
   return 1;
}

const char * vcos_thread_get_name(const VCOS_THREAD_T *thread)
{
   return thread->name;
}

#ifdef VCOS_HAVE_BACKTRACK
void __attribute__((weak)) vcos_backtrace_self(void);
#endif

void vcos_pthreads_logging_assert(const char *file, const char *func, unsigned int line, const char *fmt, ...)
{
   va_list ap;
#ifdef WIN32_KERN
   UNREFERENCED_PARAMETER(file);
   UNREFERENCED_PARAMETER(func);
   UNREFERENCED_PARAMETER(line);

   va_start(ap, fmt);

   DbgPrint(fmt, ap);

   va_end(ap);

#ifdef VCOS_HAVE_BACKTRACK
   if (vcos_backtrace_self)
      vcos_backtrace_self();
#endif
   __debugbreak();
#else
   va_start(ap, fmt);
   fprintf(stderr, "assertion failure:%s:%d:%s():",
       file, line, func);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   fprintf(stderr, "\n");

#ifdef VCOS_HAVE_BACKTRACK
   if (vcos_backtrace_self)
       vcos_backtrace_self();
#endif
   abort();
#endif
}

extern VCOS_STATUS_T vcos_thread_at_exit(void (*pfn)(void*), void *cxt)
{
   int i;
   VCOS_THREAD_T *self = vcos_thread_current();
   if (!self)
   {
      vcos_assert(0);
      return VCOS_EINVAL;
   }
   for (i=0; i<VCOS_MAX_EXIT_HANDLERS; i++)
   {
      if (self->at_exit[i].pfn == NULL)
      {
         self->at_exit[i].pfn = pfn;
         self->at_exit[i].cxt = cxt;
         return VCOS_SUCCESS;
      }
   }
   return VCOS_ENOSPC;
}

void vcos_set_args(int argc, const char **argv)
{
   vcos_argc = argc;
   vcos_argv = argv;
}

int vcos_get_argc(void)
{
   return vcos_argc;
}

const char ** vcos_get_argv(void)
{
   return vcos_argv;
}

/* we can't inline this, because HZ comes from sys/param.h which
 * dumps all sorts of junk into the global namespace, notable MIN and
 * MAX.
 */
uint32_t _vcos_get_ticks_per_second(void)
{
    // TODO implement
    return 9000000;
}

VCOS_STATUS_T vcos_once(VCOS_ONCE_T *once_control,
                        void (*init_routine)(void))
{
#ifdef WIN32_KERN
    __try
    {
        #pragma warning(suppress : 4152 )
        if (RtlRunOnceExecuteOnce (once_control, InitHandleFunction, init_routine, NULL) != STATUS_SUCCESS) {
            return VCOS_EINVAL;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return VCOS_EINVAL;
    }

    return VCOS_SUCCESS;
#else
#pragma warning(suppress : 4152 )
    if (InitOnceExecuteOnce(once_control, InitHandleFunction, init_routine, NULL) == 0) {
        return VCOS_EINVAL;
    }

    return VCOS_SUCCESS;
#endif
}


VCOS_THREAD_T *vcos_dummy_thread_create(void)
{

   VCOS_STATUS_T st;
   VCOS_THREAD_T *thread_hndl = NULL;
   int rc;

   thread_hndl = (VCOS_THREAD_T *)vcos_malloc(sizeof(VCOS_THREAD_T), NULL);
   vcos_assert(thread_hndl != NULL);

   memset(thread_hndl, 0, sizeof(VCOS_THREAD_T));

   thread_hndl->dummy = 1;
   thread_hndl->thread = vcos_llthread_current();

   st = vcos_semaphore_create(&thread_hndl->suspend, NULL, 0);
   if (st != VCOS_SUCCESS)
   {
      vcos_free(thread_hndl);
      return( thread_hndl );
   }

   vcos_once(&current_thread_key_once, current_thread_key_init);

#ifdef WIN32_KERN 
   // TODO : Implement kenel mode implementation
#else
   TlsSetValue(_vcos_thread_current_key, thread_hndl);
#endif
   (void)rc;

   return(thread_hndl);
}


/***********************************************************
 *
 * Timers
 *
 ***********************************************************/

/* On Linux we could use POSIX timers with a bit of synchronization.
 * Unfortunately POSIX timers on Bionic are NOT POSIX compliant
 * what makes that option not viable.
 * That's why we ended up with our own implementation of timers.
 * NOTE: That condition variables on Bionic are also buggy and
 * they work incorrectly with CLOCK_MONOTONIC, so we have to
 * use CLOCK_REALTIME (and hope that no one will change the time
 * significantly after the timer has been set up
 */
#define NSEC_IN_SEC  (1000*1000*1000)
#define MSEC_IN_SEC  (1000)
#define NSEC_IN_MSEC (1000*1000)

static int _timespec_is_zero(struct timespec *ts)
{
   return ((ts->tv_sec == 0) && (ts->tv_nsec == 0));
}

static void _timespec_set_zero(struct timespec *ts)
{
   ts->tv_sec = ts->tv_nsec = 0;
}

/* Adds left to right and stores the result in left */
static void _timespec_add(struct timespec *left, struct timespec *right)
{
   left->tv_sec += right->tv_sec;
   left->tv_nsec += right->tv_nsec;
   if (left->tv_nsec >= (NSEC_IN_SEC))
   {
      left->tv_nsec -= NSEC_IN_SEC;
      left->tv_sec++;
   }
}

static int _timespec_is_larger(struct timespec *left, struct timespec *right)
{
   if (left->tv_sec != right->tv_sec)
      return left->tv_sec > right->tv_sec;
   else
      return left->tv_nsec > right->tv_nsec;
}

static void* _timer_thread(void *arg)
{
    //TODO Implement
    UNREFERENCED_PARAMETER(arg);
    vcos_assert(FALSE);
    return NULL;
}

VCOS_STATUS_T vcos_timer_init(void)
{
   return VCOS_SUCCESS;
}

VCOS_STATUS_T vcos_timer_create(VCOS_TIMER_T *timer,
                                const char *name,
                                void (*expiration_routine)(void *context),
                                void *context)
{
    // TODO Implement
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(expiration_routine);
    UNREFERENCED_PARAMETER(timer);
    vcos_assert(FALSE);

    return VCOS_EEXIST;
}

void vcos_timer_set(VCOS_TIMER_T *timer, VCOS_UNSIGNED delay_ms)
{
    // TODO Implement
    UNREFERENCED_PARAMETER(timer);
    UNREFERENCED_PARAMETER(delay_ms);
    vcos_assert(FALSE);
    return;
}

void vcos_timer_cancel(VCOS_TIMER_T *timer)
{
    // TODO Implement
    UNREFERENCED_PARAMETER(timer);
    vcos_assert(FALSE);
}

void vcos_timer_reset(VCOS_TIMER_T *timer, VCOS_UNSIGNED delay_ms)
{
   vcos_timer_set(timer, delay_ms);
}

void vcos_timer_delete(VCOS_TIMER_T *timer)
{
    // TODO Implement
    UNREFERENCED_PARAMETER(timer);
    vcos_assert(FALSE);
}

