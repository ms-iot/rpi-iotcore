/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name: 

    spitrace.h

Abstract:

    This module contains the trace definitions for 
    the BCM2836 SPI0 controller driver.

Environment:

    kernel-mode only

Revision History:

--*/

#ifndef _SPITRACE_H_
#define _SPITRACE_H_

extern "C" 
{
//
// Tracing Definitions:
//
// Control GUID: 
// {383D2181-9EED-4FC9-83EC-68F806D4498D}

#define WPP_CONTROL_GUIDS                           \
    WPP_DEFINE_CONTROL_GUID(                        \
        PbcTraceGuid,                               \
        (383D2181, 9EED, 4fc9, 83EC, 68F806D4498D), \
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

#endif // _SPITRACE_H_
