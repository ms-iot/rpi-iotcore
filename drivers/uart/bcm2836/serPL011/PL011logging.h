//
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011logging.h
//
// Abstract:
//
//    This module contains the tracing definitions for the 
//    ARM PL011 (UART) device driver.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//

#ifndef _PL011_LOGGING_H_
#define _PL011_LOGGING_H_

WDF_EXTERN_C_START


//
// Defining control guids, including this is required to happen before
// including the tmh file (if the WppRecorder API is used)
//
#include <WppRecorder.h>

//
// Debug support
//
extern BOOLEAN PL011IsDebuggerPresent();
extern BOOLEAN PL011BreakPoint();


//
// Tracing GUID - 2EA62EE7-3DC8-401D-99DA-3F9379A231AD
//
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(SERPL011, (2EA62EE7,3DC8,401D,99DA,3F9379A231AD), \
        WPP_DEFINE_BIT(PL011_TRACING_DEFAULT) \
        WPP_DEFINE_BIT(PL011_TRACING_VERBOSE) \
        WPP_DEFINE_BIT(PL011_TRACING_DEBUG) \
    )

// begin_wpp config
//
// FUNC PL011_LOG_ASSERTION{LEVEL=TRACE_LEVEL_ERROR, FLAGS=PL011_TRACING_DEBUG}(MSG, ...);
// USEPREFIX (PL011_LOG_ASSERTION, "%!STDPREFIX! [%s @ %u] ASSERTION :", __FILE__, __LINE__);
//
// FUNC PL011_LOG_ERROR{LEVEL=TRACE_LEVEL_ERROR, FLAGS=PL011_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (PL011_LOG_ERROR, "%!STDPREFIX! [%s @ %u] ERROR :", __FILE__, __LINE__);
//
// FUNC PL011_LOG_WARNING{LEVEL=TRACE_LEVEL_WARNING, FLAGS=PL011_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (PL011_LOG_WARNING, "%!STDPREFIX! [%s @ %u] WARNING :", __FILE__, __LINE__);
//
// FUNC PL011_LOG_INFORMATION{LEVEL=TRACE_LEVEL_INFORMATION, FLAGS=PL011_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (PL011_LOG_INFORMATION, "%!STDPREFIX! [%s @ %u] INFO :", __FILE__, __LINE__);
//
// FUNC PL011_LOG_TRACE{LEVEL=TRACE_LEVEL_VERBOSE, FLAGS=PL011_TRACING_VERBOSE}(MSG, ...);
// USEPREFIX (PL011_LOG_TRACE, "%!STDPREFIX! [%s @ %u] TRACE :", __FILE__, __LINE__);
//
// FUNC PL011_ASSERT{LEVEL=TRACE_LEVEL_ERROR, FLAGS=PL011_TRACING_DEBUG}(PL011_ASSERT_EXP);
// USEPREFIX (PL011_ASSERT, "%!STDPREFIX! [%s @ %u] ASSERTION :%s", __FILE__, __LINE__, #PL011_ASSERT_EXP);
//
// end_wpp


//
// PL011_ASSERT customization
//
#define WPP_RECORDER_LEVEL_FLAGS_PL011_ASSERT_EXP_FILTER(LEVEL, FLAGS, PL011_ASSERT_EXP) \
    (!(PL011_ASSERT_EXP))

#define WPP_RECORDER_LEVEL_FLAGS_PL011_ASSERT_EXP_ARGS(LEVEL, FLAGS, PL011_ASSERT_EXP) \
    WPP_CONTROL(WPP_BIT_ ## FLAGS).AutoLogContext, LEVEL, WPP_BIT_ ## FLAGS

#define WPP_LEVEL_FLAGS_PL011_ASSERT_EXP_POST(LEVEL, FLAGS, PL011P_ASSERT_EXP) \
    ,((!(PL011P_ASSERT_EXP)) ? PL011BreakPoint() : 1)


WDF_EXTERN_C_END

#endif // !_PL011_LOGGING_H_