//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011common.h
//
// Abstract:
//
//    This module contains common enum, types, and other definitions common
//    to the PL011 UART device driver.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//

#ifndef _PL011_COMMON_H_
#define _PL011_COMMON_H_

WDF_EXTERN_C_START

// Common SerPL011 driver header files
#include "PL011uart.h"
#include "PL011device.h"
#include "PL011hw.h"


//
// PL011 pool allocation tags
//
enum class PL011_ALLOC_TAG : ULONG {

    PL011_ALLOC_TAG_TEMP    = '01LP', // Temporary be freed in the same routine
    PL011_ALLOC_TAG_WDF      = '@1LP'  // Allocations WDF makes on our behalf

}; // enum PL011_ALLOC_TAG


//
// PL011common public methods
//


//
// Routine Description:
//
//  PL011StateSet is called to set the next state.
//
// Arguments:
//
//  StateVarPtr - Address of the state variable.
//
//  NextState - Next state.
//
//  StateStr - The array of state description strings
//      where the first entry is the preamble.
//
//  StateStrCount - Number of elements in StateStr
//
// Return Value:
//  
//  The previous state.
//
#if DBG
    ULONG
    PL011StateSet(
        _In_ ULONG* StateVarPtr,
        _In_ ULONG  NextState,
        _In_count_(StateStrCount) const CHAR* StateStr[],
        _In_ SIZE_T StateStrCount
        );
#else // DBG
    __forceinline
    ULONG
    PL011StateSet(
        _In_ ULONG* StateVarPtr,
        _In_ ULONG NextState
        )
    {
        ULONG prevState = ULONG(InterlockedExchange(
            reinterpret_cast<volatile LONG*>(StateVarPtr),
            LONG(NextState)
            ));

        return prevState;
    }
#endif // !DBG


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
//  NextState - Next state.
//
//  CompareState - The state to compare with.
//
//  StateStr - The array of state description strings
//      where the first entry is the preamble.
//
//  StateStrCount - Number of elements in StateStr
//
// Return Value:
//  
//  TRUE if state was set to NextState, otherwise FALSE.
//
#if DBG
    BOOLEAN
    PL011StateSetCompare(
        _In_ ULONG* StateVarPtr,
        _In_ ULONG NextState,
        _In_ ULONG CompareState,
        _In_count_(StateStrCount) const CHAR* StateStr[],
        _In_ SIZE_T StateStrCount
        );
#else // DBG
    __forceinline
    BOOLEAN
    PL011StateSetCompare(
        _In_ ULONG* StateVarPtr,
        _In_ ULONG NextState,
        _In_ ULONG CompareState
        )
    {
        ULONG prevState = ULONG(InterlockedCompareExchange(
            reinterpret_cast<volatile LONG*>(StateVarPtr),
            LONG(NextState),
            LONG(CompareState)
            ));

        return prevState == CompareState;
    }
#endif // !DBG


//
// Routine Description:
//
//  PL011StateGet is called to get the current state.
//
// Arguments:
//
//  StateVarPtr - Address of the state variable.
//
// Return Value:
//
//  The current state.
//
__forceinline
ULONG
PL011StateGet(
    ULONG* StateVarPtr
    )
{
    ULONG curState = ULONG(InterlockedAdd(
        reinterpret_cast<volatile LONG*>(StateVarPtr),
        0
        ));

    return curState;
}


WDF_EXTERN_C_END


#endif // !_PL011_COMMON_H_
