//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011common.h
//
// Abstract:
//
//    This module contains implementation of common methods
//    used throughout the PL011 UART device driver.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//
#include "precomp.h"
#pragma hdrstop

#define _PL011_COMMON_CPP_

// Logging
#include "PL011logging.h"
#include "PL011common.tmh"

// Module specific header files
#include "PL011common.h"


//
// Routine Description:
//
//  PL011StateSet is called to set the next state.
//
// Arguments:
//
//  StateVarPtr - Address of the state variable.
//
//  NextState - Next state
//
//  StateStr - The array of state description strings
//      where the first entry is the preamble.
//
//  StateStrCount - Number of elements in StateStr
//
// Return Value:
//  
//  The previous state
//
#if DBG
    _Use_decl_annotations_
    ULONG
    PL011StateSet(
        ULONG* StateVarPtr,
        ULONG NextState,
        const CHAR* StateStr[],
        SIZE_T StateStrCount
        )
    {
        ULONG prevState = ULONG(InterlockedExchange(
            reinterpret_cast<volatile LONG*>(StateVarPtr),
            LONG(NextState)
            ));

        PL011_ASSERT((NextState + 1) < StateStrCount);

        __analysis_assume((NextState + 1) < StateStrCount);

        PL011_LOG_TRACE(
            "%s State Set: previous %s, current %s",
            StateStr[0],
            StateStr[prevState + 1],
            StateStr[*StateVarPtr + 1]
            );

        return prevState;
    }
#endif // DBG


//
// Routine Description:
//
//  PL011StateSetCompare is called to set the next state, if
//  the current state is equal to the comparand state.
//
// Arguments:
//
//  StateVarPtr - Address of the state variable.
//
//  NextRxPioState - The next RX PIO state.
//
//  ComparetRxPioState - The RX PIO state to compare with.
//
//  StateStr - The array of state description strings
//      where the first entry is the preamble.
//
//  StateStrCount - Number of elements in StateStr
//
// Return Value:
//  
//  TRUE if NextState was set, otherwise FALSE.
//
#if DBG
    _Use_decl_annotations_
    BOOLEAN
    PL011StateSetCompare(
        ULONG* StateVarPtr,
        ULONG NextState,
        ULONG CompareState,
        const CHAR* StateStr[],
        SIZE_T StateStrCount
        )
    {
        ULONG prevState = ULONG(InterlockedCompareExchange(
                reinterpret_cast<volatile LONG*>(StateVarPtr),
                LONG(NextState),
                LONG(CompareState)
                ));

        PL011_ASSERT((NextState + 1) < StateStrCount);
        PL011_ASSERT((CompareState + 1) < StateStrCount);

        __analysis_assume((NextState + 1) < StateStrCount);
        __analysis_assume((CompareState + 1) < StateStrCount);

        PL011_LOG_TRACE(
            "%s State Set Compare: previous %s, current %s, if previous %s",
            StateStr[0],
            StateStr[prevState + 1],
            StateStr[*StateVarPtr + 1],
            StateStr[CompareState + 1]
            );

        return prevState == CompareState;
    }
#endif


//
// Routine Description:
//
//  PL011IsDebuggerPresent is called to check if the debugger is present
//  and enabled.
//
// Arguments:
//
// Return Value:
//  
//  TRUE debugger is present, otherwise FALSE.
//
BOOLEAN
PL011IsDebuggerPresent()
{
    static LONG isDebuggerStateUpToDate = FALSE;

    if (InterlockedExchange(&isDebuggerStateUpToDate, TRUE) == 0) {

        KdRefreshDebuggerNotPresent();
    }

    return (KD_DEBUGGER_ENABLED && !KD_DEBUGGER_NOT_PRESENT);
}


//
// Routine Description:
//
//  PL011BreakPoint is called by PL011_ASSERT to break if debugger
//  is present and enabled.
//
// Arguments:
//
// Return Value:
//
//  Always TRUE to continue...
//
BOOLEAN
PL011BreakPoint()
{
    if (PL011IsDebuggerPresent()) {

        DbgBreakPoint();
    }

    return TRUE;
}


#undef _PL011_COMMON_CPP_
