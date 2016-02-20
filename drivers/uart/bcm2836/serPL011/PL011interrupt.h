//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011interrupt.h
//
// Abstract:
//
//    This module contains common enum, types, and methods definitions
//    for handling ARM PL011 UART interrupts.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//

#ifndef _PL011_INTERRUPT_H_
#define _PL011_INTERRUPT_H_

WDF_EXTERN_C_START


//
// PL011interrupt WDF interrupt event handlers
//
EVT_WDF_INTERRUPT_ISR PL011EvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC PL011EvtInterruptDpc;


//
// PL011interrupt private methods
//
#ifdef _PL011_INTERRUPT_CPP_

    BOOLEAN
    PL011pInterruptIsr(
        _In_ PL011_DEVICE_EXTENSION* DevExtPtr
        );

#endif //_PL011_INTERRUPT_CPP_


WDF_EXTERN_C_END

#endif // !_PL011_INTERRUPT_H_
