/*++

Copyright (c) 1993  Microsoft Corporation
:ts=4

Module Name:

    log.h

Abstract:

    debug macros

Environment:

    Kernel & user mode

--*/

#ifndef   __LOG_H__
#define   __LOG_H__

//#if !defined(EVENT_TRACING)

#if FALSE
VOID
TraceEvents    (
    IN ULONG   DebugPrintLevel,
    IN ULONG   DebugPrintFlag,
    IN PCCHAR  DebugMessage,
    ...
    );
#else
#define TraceEvents(DebugPrintLevel,DebugPrintFlag,DebugMessage) TraceEvents((TraceEventsLevel), (TraceEventsFlag), (DebugMessage))
#endif

//#endif

#endif // __LOG_H__


