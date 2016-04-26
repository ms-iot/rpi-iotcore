//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     trace.h
//
// Abstract:
//
//     Header file for the debug tracing related function defintions and macros.
//

#include <wpprecorder.h>
//
// Define the tracing flags.
//
// Tracing GUID - 32EC38BE-1C63-47CD-ABEE-7D4E0ACDACAF
//

#define WPP_CONTROL_GUIDS                                                       \
     WPP_DEFINE_CONTROL_GUID(                                                   \
          VchiqTraceGuid, (05B041F1,D0E4,4AA6,A9F6,ECBEBC832D4C),               \
          WPP_DEFINE_BIT(TRACE_INFO)                                            \
          WPP_DEFINE_BIT(TRACE_WARNING)                                         \
          WPP_DEFINE_BIT(TRACE_ERROR)                                           \
          )                                      

#define WPP_FLAG_LEVEL_LOGGER(flag, level)                                      \
     WPP_LEVEL_LOGGER(flag)

#define WPP_FLAG_LEVEL_ENABLED(flag, level)                                     \
     (WPP_LEVEL_ENABLED(flag) &&                                                \
      WPP_CONTROL(WPP_BIT_ ## flag).Level >= level)

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags)                                       \
              WPP_LEVEL_LOGGER(flags)
                    
#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags)                                     \
              (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= lvl)

//
// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// begin_wpp config
//
// FUNC VCHIQ_LOG_INFORMATION{LEVEL=TRACE_LEVEL_INFORMATION, FLAGS=TRACE_INFO}(MSG, ...);
// USEPREFIX (VCHIQ_LOG_INFORMATION, "%!STDPREFIX! [%s @ %u] INFO:", __FILE__, __LINE__);
//
// FUNC VCHIQ_LOG_WARNING{LEVEL=TRACE_LEVEL_WARNING, FLAGS=TRACE_WARNING}(MSG, ...);
// USEPREFIX (VCHIQ_LOG_WARNING, "%!STDPREFIX! [%s @ %u] WARNING:", __FILE__, __LINE__);
//
// FUNC VCHIQ_LOG_ERROR{LEVEL=TRACE_LEVEL_CRITICAL, FLAGS=TRACE_ERROR}(MSG, ...);
// USEPREFIX (VCHIQ_LOG_ERROR, "%!STDPREFIX! [%s @ %u] CRITICAL ERROR:", __FILE__, __LINE__);
//
// end_wpp
//

extern ULONG VchiqCriticalDebugBreak();

#define WPP_LEVEL_FLAGS_POST(LEVEL,FLAGS) \
    ,(((WPP_BIT_ ## FLAGS) == WPP_BIT_TRACE_ERROR) ? \
            VchiqCriticalDebugBreak() : 1)
