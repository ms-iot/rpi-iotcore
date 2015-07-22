/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name: 

    i2ctrace.h

Abstract:

    This module contains the trace definitions for 
    the I2C controller driver.

Environment:

    kernel-mode only

Revision History:

--*/

#ifndef _I2CTRACE_H_
#define _I2CTRACE_H_

extern "C" 
{
//
// Tracing Definitions:
//
// Control GUID: 
// {2C6CF78D-93D0-4A18-A3A5-49C67BCBF820}

#define WPP_CONTROL_GUIDS                           \
    WPP_DEFINE_CONTROL_GUID(                        \
        PbcTraceGuid,                               \
        (2C6CF78D,93D0,4A18,A3A5,49C67BCBF820),     \
        WPP_DEFINE_BIT(TRACE_FLAG_WDFLOADING)       \
        WPP_DEFINE_BIT(TRACE_FLAG_SPBDDI)           \
        WPP_DEFINE_BIT(TRACE_FLAG_PBCLOADING)       \
        WPP_DEFINE_BIT(TRACE_FLAG_TRANSFER)         \
        WPP_DEFINE_BIT(TRACE_FLAG_OTHER)            \
        )
}

#define WPP_LEVEL_FLAGS_LOGGER(level,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(level, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= level)

// begin_wpp config
// FUNC FuncEntry{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// FUNC FuncExit{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// USEPREFIX(FuncEntry, "%!STDPREFIX! [%!FUNC!] --> entry");
// USEPREFIX(FuncExit, "%!STDPREFIX! [%!FUNC!] <--");
// end_wpp

#endif // _I2CTRACE_H_
