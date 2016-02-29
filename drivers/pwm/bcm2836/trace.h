/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Abstract:

This file contains definitions for tracing.

--*/


//
// Define the tracing flags.
//
// Tracing GUID - dcc0f520-7600-42d0-b1e4-f968c4597781
//

#define WPP_CONTROL_GUIDS                                                   \
    WPP_DEFINE_CONTROL_GUID(                                                \
        BCM_PWM_TraceGuid, (dcc0f520,7600,42d0,b1e4,f968c4597781),          \
        WPP_DEFINE_BIT(TRACE_INIT)                                          \
        WPP_DEFINE_BIT(TRACE_DEVICE)                                        \
        WPP_DEFINE_BIT(TRACE_IOCTL)                                         \
        WPP_DEFINE_BIT(TRACE_IO)                                            \
        WPP_DEFINE_BIT(TRACE_IRQ)                                           \
        WPP_DEFINE_BIT(TRACE_FUNC)                                          \
        )                             

#define WPP_FLAG_LEVEL_LOGGER(flag, level)                                  \
    WPP_LEVEL_LOGGER(flag)

#define WPP_FLAG_LEVEL_ENABLED(flag, level)                                 \
    (WPP_LEVEL_ENABLED(flag) &&                                             \
     WPP_CONTROL(WPP_BIT_ ## flag).Level >= level)

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags)                                   \
           WPP_LEVEL_LOGGER(flags)

#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags)                                 \
           (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= lvl)

#define WPP__ENABLED()                                                      \
           WPP_LEVEL_ENABLED(TRACE_FUNC)

#define WPP__LOGGER()                                                       \
           WPP_LEVEL_LOGGER(TRACE_FUNC) 

//
// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// begin_wpp config
// FUNC Trace{FLAG=BCM_PWM__ALL}(LEVEL, MSG, ...);
// FUNC TraceEvents(LEVEL, FLAGS, MSG, ...);
// FUNC FuncEntry();
// FUNC FuncExit();
// USESUFFIX(FuncEntry, "[%!FUNC!] --> entry");
// USESUFFIX(FuncExit, "[%!FUNC!] <-- exit");
// end_wpp
//


