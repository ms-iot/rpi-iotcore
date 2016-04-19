#ifndef _BCM_AUXSPI_TRACE_H_
#define _BCM_AUXSPI_TRACE_H_ 1

//
// Copyright (C) Microsoft.  All rights reserved.
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
// Tracing GUID - F40BAF8A-C1D6-457F-8AB1-04E29BA7DB0B
//
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(BCMAUXSPI, (F40BAF8A,C1D6,457F,8AB1,04E29BA7DB0B), \
        WPP_DEFINE_BIT(AUXSPI_TRACING_DEFAULT) \
    )

// begin_wpp config
//
// FUNC AUXSPI_LOG_ERROR{LEVEL=TRACE_LEVEL_ERROR, FLAGS=AUXSPI_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (AUXSPI_LOG_ERROR, "%!STDPREFIX! [%s @ %u] ERROR :", __FILE__, __LINE__);
//
// FUNC AUXSPI_LOG_LOW_MEMORY{LEVEL=TRACE_LEVEL_ERROR, FLAGS=AUXSPI_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (AUXSPI_LOG_LOW_MEMORY, "%!STDPREFIX! [%s @ %u] LOW MEMORY :", __FILE__, __LINE__);
//
// FUNC AUXSPI_LOG_WARNING{LEVEL=TRACE_LEVEL_WARNING, FLAGS=AUXSPI_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (AUXSPI_LOG_WARNING, "%!STDPREFIX! [%s @ %u] WARNING :", __FILE__, __LINE__);
//
// FUNC AUXSPI_LOG_INFORMATION{LEVEL=TRACE_LEVEL_INFORMATION, FLAGS=AUXSPI_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (AUXSPI_LOG_INFORMATION, "%!STDPREFIX! [%s @ %u] INFO :", __FILE__, __LINE__);
//
// FUNC AUXSPI_LOG_TRACE{LEVEL=TRACE_LEVEL_VERBOSE, FLAGS=AUXSPI_TRACING_DEFAULT}(MSG, ...);
// USEPREFIX (AUXSPI_LOG_TRACE, "%!STDPREFIX! [%s @ %u] TRACE :", __FILE__, __LINE__);
//
// end_wpp

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // _BCM_AUXSPI_TRACE_H_

