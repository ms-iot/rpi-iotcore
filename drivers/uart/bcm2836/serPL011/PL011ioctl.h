//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011ioctl.h
//
// Abstract:
//
//    This module contains enum, types, methods, and IOCTL handlers
//    definitions associated with the ARM PL011 UART device driver IOCTL 
//    processing
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//

#ifndef _PL011_IOCTL_H_
#define _PL011_IOCTL_H_

WDF_EXTERN_C_START


//
// PL011ioctl public methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetBaudRate(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlGetBaudRate(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetHandflow(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlGetHandflow(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetModemControl(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlGetModemControl(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetLineControl(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlGetLineControl(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlGetChars(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlClrRts(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetRts(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlClrDtr(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetDtr(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlGetDtrRts(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlGetProperties(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetBreakOff(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetBreakOn(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetBreakOn(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlGetCommStatus(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlGetModemStatus(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011IoctlSetFifoControl(
    _In_ WDFDEVICE WdfDevice,
    _In_ WDFREQUEST WdfRequest
    );


//
// PL011ioctl private methods
//
#ifdef _PL011_IOCTL_CPP_

#endif //_PL011_IOCTL_CPP_


WDF_EXTERN_C_END

#endif // !_PL011_IOCTL_H_
