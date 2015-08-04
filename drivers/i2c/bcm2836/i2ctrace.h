#ifndef _I2CTRACE_H_
#define _I2CTRACE_H_ 1

//
// Copyright (C) Microsoft. All rights reserved.
//

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//
// Defining control guids, including this is required to happen before
// including the tmh file (if the WppRecorder API is used)
//
#include <WppRecorder.h>

//
// Tracing GUID - 2C6CF78D-93D0-4A18-A3A5-49C67BCBF820
//
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(BCMI2C, (2C6CF78D,93D0,4A18,A3A5,49C67BCBF820), \
        WPP_DEFINE_BIT(BSC_TRACING_DEFAULT) \
        WPP_DEFINE_BIT(BSC_TRACING_VERBOSE) \
        WPP_DEFINE_BIT(BSC_TRACING_DEBUG) \
        WPP_DEFINE_BIT(BSC_TRACING_BUGCHECK) \
    )

// begin_wpp config
//
// FUNC BSC_LOG_ERROR{LEVEL=TRACE_LEVEL_ERROR, FLAGS=BSC_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (BSC_LOG_ERROR, "%!STDPREFIX! [%s @ %u] ERROR :", __FILE__, __LINE__);
//
// FUNC BSC_LOG_LOW_MEMORY{LEVEL=TRACE_LEVEL_ERROR, FLAGS=BSC_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (BSC_LOG_LOW_MEMORY, "%!STDPREFIX! [%s @ %u] LOW MEMORY :", __FILE__, __LINE__);
//
// FUNC BSC_LOG_WARNING{LEVEL=TRACE_LEVEL_WARNING, FLAGS=BSC_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (BSC_LOG_WARNING, "%!STDPREFIX! [%s @ %u] WARNING :", __FILE__, __LINE__);
//
// FUNC BSC_LOG_INFORMATION{LEVEL=TRACE_LEVEL_INFORMATION, FLAGS=BSC_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (BSC_LOG_INFORMATION, "%!STDPREFIX! [%s @ %u] INFO :", __FILE__, __LINE__);
//
// FUNC BSC_LOG_TRACE{LEVEL=TRACE_LEVEL_VERBOSE, FLAGS=BSC_TRACING_VERBOSE}(MSG, ...);
// USEPREFIX (BSC_LOG_TRACE, "%!STDPREFIX! [%s @ %u] TRACE :", __FILE__, __LINE__);
//
// end_wpp

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // _I2CTRACE_H_

