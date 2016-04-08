/*++

Copyright (c) Microsoft Corporation.

Module Name:

    Trace.h

Abstract:

    Header file for the debug tracing related function defintions and macros.

Environment:

    Kernel mode

--*/

#pragma once


//
// Debugging Output Levels
//
#ifndef TRACE_LEVEL_INFORMATION
#define TRACE_LEVEL_NONE                0   // Tracing is not on
#define TRACE_LEVEL_FATAL               1   // Abnormal exit or termination
#define TRACE_LEVEL_ERROR               2   // Severe errors that need logging
#define TRACE_LEVEL_WARNING             3   // Warnings such as allocation failure
#define TRACE_LEVEL_INFORMATION         4   // Includes non-error cases(e.g.,Entry-Exit)
#define TRACE_LEVEL_VERBOSE             5   // Detailed traces from intermediate steps
#define TRACE_LEVEL_RESERVED6           6
#define TRACE_LEVEL_RESERVED7           7
#define TRACE_LEVEL_RESERVED8           8
#define TRACE_LEVEL_RESERVED9           9
#endif  //  TRACE_LEVEL_INFORMATION



//
// Define the tracing flags
//
// Tracing GUID - 68e52676-b413-4d63-a1b4-a115d1aef312
//
#ifdef  WPP_TRACING
//
// Defining control guids, including this is required to happen before
// including the tmh file (if the WppRecorder API is used)
//
#include <WppRecorder.h>


#define WPP_CONTROL_GUIDS                                                                \
    WPP_DEFINE_CONTROL_GUID(Bcm2836SDHCTraceGuid, (68e52676,b413,4d63,a1b4,a115d1aef312),\
                            WPP_DEFINE_BIT(DRVR_LVL_ERR)                                 \
                            WPP_DEFINE_BIT(DRVR_LVL_WARN)                                \
                            WPP_DEFINE_BIT(DRVR_LVL_INFO)                                \
                            WPP_DEFINE_BIT(DRVR_LVL_FUNC))


#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) \
    WPP_LEVEL_LOGGER(flags)


#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) \
    (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= lvl)

#else   // WPP_TRACING


extern ULONG DefaultDebugLevel;
extern ULONG DefaultDebugFlags;


#define DRVR_LVL_ERR         0x00000001
#define DRVR_LVL_WARN        0x00000002
#define DRVR_LVL_INFO        0x00000004
#define DRVR_LVL_FUNC        0x00000008


#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) \
    (DefaultDebugLevel >= (lvl) &&          \
        DefaultDebugFlags & (flags))


#define TraceMessage(_level_,_flag_,_msg_)  \
    if (DefaultDebugLevel >= (_level_) &&   \
        DefaultDebugFlags & (_flag_)) {     \
        DbgPrint ("Bcm2836SDHC: ");          \
        DbgPrint _msg_;                     \
        DbgPrint("\n");                     \
    }

static __inline VOID
OutputDebugMessage (
    __drv_formatString(printf) __in PCSTR DebugMessage,
    ...
    )
{
    va_list ArgList;
    va_start(ArgList, DebugMessage);

    DbgPrintEx (DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, "%s", "Bcm2836SDHC: ");
    vDbgPrintEx (DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, DebugMessage, ArgList);

    va_end(ArgList);
    return;
}

#define BOOL2TEXT(_flag_) BOOLIFY(_flag_) ? "enabled" : "disabled"

#endif  // WPP_TRACING


//
// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// begin_wpp config
// FUNC Trace{FLAG=MYDRIVER_ALL_INFO}(LEVEL, MSG, ...);
// FUNC TraceEvents(LEVEL, FLAGS, MSG, ...);
// end_wpp
//
