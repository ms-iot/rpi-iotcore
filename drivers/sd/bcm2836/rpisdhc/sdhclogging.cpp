//
// Copyright (C) Microsoft. All rights reserved.
//

#include "precomp.hpp"
#pragma hdrstop

#include "SdhcLogging.h"
#include "SdhcLogging.tmh"

namespace { // static

    bool isDebuggerPresent (
        bool Refresh = false
        ) throw ()
    {
        //
        // NOTE: we do not care about possible multithreading issues here as in
        // the worst case we will refresh debuggers status more than once. Such
        // negligible side-effect doesn't warrant complexity and performance "tax"
        // associated with "proper" synchronization...
        //
        static bool debuggerPresenceInitialized = false;

        if (!debuggerPresenceInitialized) {
            KdRefreshDebuggerNotPresent();
            debuggerPresenceInitialized = true;
        } else if (Refresh != false) {
            KdRefreshDebuggerNotPresent();
        } // iff

        return (KD_DEBUGGER_ENABLED && !KD_DEBUGGER_NOT_PRESENT);
    } // isDebuggerPresent (...)

} // namespace static

_Use_decl_annotations_
void SDHC_LOG_INIT (
    DRIVER_OBJECT* DriverObjectPtr,
    UNICODE_STRING* RegistryPathPtr
    )
{
    WPP_INIT_TRACING(DriverObjectPtr, RegistryPathPtr);

    RECORDER_CONFIGURE_PARAMS recorderConfigureParams;
    RECORDER_CONFIGURE_PARAMS_INIT(&recorderConfigureParams);
    WppRecorderConfigure(&recorderConfigureParams);

#if DBG
    {
        RECORDER_LOG_CREATE_PARAMS recorderLogCreateParams;
        RECORDER_LOG_CREATE_PARAMS_INIT(&recorderLogCreateParams, "TraceLog");
        // NOTE: Actual log size may be adjusted down by WPP
        recorderLogCreateParams.TotalBufferSize = (32 * 1024);
        recorderLogCreateParams.ErrorPartitionSize = 0;
        NTSTATUS status = WppRecorderLogCreate(
                &recorderLogCreateParams,
                &_SdhcLogTraceRecorder);
        if (!NT_SUCCESS(status)) {
            NT_ASSERT(!"Unable to create trace log recorder - default log will be used instead");
            _SdhcLogTraceRecorder = WppRecorderLogGetDefault();
        } // if
        WPP_RECORDER_LEVEL_FILTER(SDHC_TRACING_VERBOSE) = TRUE;
    }
#else
    _SdhcLogTraceRecorder = WppRecorderLogGetDefault();
#endif // DBG
} // SDHC_LOG_INIT (...)

_Use_decl_annotations_
void SDHC_LOG_CLEANUP ()
{
    // NOTE: WPP will ignore delete request for "default" log
    // - so there is no need to check for it here.
    WppRecorderLogDelete(_SdhcLogTraceRecorder);
    WPP_CLEANUP(NULL);
} // SDHC_LOG_CLEANUP ()

RECORDER_LOG _SdhcLogTraceRecorder = RECORDER_LOG();

int _SdhcLogBugcheck (
    ULONG Level
    )
{
    volatile void* returnAddress = _ReturnAddress();
#pragma prefast(suppress:__WARNING_USE_OTHER_FUNCTION, "We really-really want to bugcheck here...)")
    KeBugCheckEx(
        BUGCODE_ID_DRIVER,
        ULONG_PTR(returnAddress),
        Level,
        0,
        0);

    //return 1;
} // _SdhcLogBugcheck (...)

int _SdhcLogDebug (
    ULONG Level
    )
{
    static PCSTR levelDescriptions[] = {
        "[%s]",              // TRACE_LEVEL_NONE
        "critical error",    // TRACE_LEVEL_CRITICAL
        "noncritical error", // TRACE_LEVEL_ERROR
        "warning",           // TRACE_LEVEL_WARNING
        "information",       // TRACE_LEVEL_INFORMATION
        "trace"              // TRACE_LEVEL_VERBOSE
    }; // levelDescriptions

    volatile void* returnAddress = _ReturnAddress();
    volatile PCSTR levelDescriptionSz =
        levelDescriptions[(Level < ARRAYSIZE(levelDescriptions)) ? Level : 0];
    DbgPrintEx(
        DPFLTR_DEFAULT_ID,
        DPFLTR_ERROR_LEVEL,
        "\n*** SDHC %s detected @%p.\n",
        levelDescriptionSz,
        returnAddress);

    if (!isDebuggerPresent(false)) return 1;

    for (;;) {
        char response[2] = {0};
        DbgPrompt(
            "Break to debug, Ignore, ignore All (bi)? ",
            response,
            sizeof(response));

        if ((response[0] == 'B') || (response[0] == 'b')) {
            DbgBreakPoint();
            break;
        } else if ((response[0] == 'I') || (response[0] == 'i')) {
            break;
        } // iff
    } // for (;;)

    return 1;
} // _SdhcLogDebug (...)

