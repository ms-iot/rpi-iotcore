/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    device.h

Abstract:

    This module contains the function definitions for the
    WDF device.

Environment:

    kernel-mode only

Revision History:

--*/

#ifndef _DEVICE_H_
#define _DEVICE_H_

#define I2C_SERIAL_BUS_SPECIFIC_FLAG_10BIT_ADDRESS  0x01
#define I2C_SLV_BIT                                 0x01    // 0=initiated by controller, 1=by device
#define I2C_MAX_ADDRESS                             0x7f

//
// Optional Device Parameters REG_DWORD value to set clock stretch timeout
// in SCL clock cycles. Setting this to 0 disables clock stretch timeout.
//
#define REGSTR_VAL_CLOCK_STRETCH_TIMEOUT L"ClockStretchTimeout"

//
// I2C Serial Bus ACPI Descriptor
// See ACPI 5.0 spec table 6-192
//
#include <pshpack1.h>
struct PNP_I2C_SERIAL_BUS_DESCRIPTOR : PNP_SERIAL_BUS_DESCRIPTOR {
    ULONG ConnectionSpeed;
    USHORT Address;
    // follwed by optional Vendor Data
    // followed by PNP_IO_DESCRIPTOR_RESOURCE_NAME
};
#include <poppack.h>

//
// See section 6.4.3.8.2 of the ACPI 5.0 specification
//
enum PNP_SERIAL_BUS_TYPE {
    PNP_SERIAL_BUS_TYPE_I2C = 0x1,
    PNP_SERIAL_BUS_TYPE_SPI = 0x2,
    PNP_SERIAL_BUS_TYPE_UART = 0x3,
};

enum TRANSFER_STATE : ULONG {
    INVALID,
    SENDING,
    RECEIVING,
    SENDING_SEQUENCE,
    RECEIVING_SEQUENCE,
    SENDING_WAIT_FOR_DONE,
    RECEIVING_WAIT_FOR_DONE,
    RECEIVING_SEQUENCE_WAIT_FOR_DONE,
    ERROR_FLAG = 0x80000000UL,
};

struct BCM_I2C_INTERRUPT_CONTEXT;

struct BCM_I2C_DEVICE_CONTEXT {
    BCM_I2C_REGISTERS* RegistersPtr;
    BCM_I2C_INTERRUPT_CONTEXT* InterruptContextPtr;
    WDFDEVICE  WdfDevice;
    WDFINTERRUPT WdfInterrupt;
    PHYSICAL_ADDRESS RegistersPhysicalAddress;
    ULONG RegistersLength;
    ULONG ClockStretchTimeout;  // in units of SCL clock cycles
};

struct BCM_I2C_TARGET_CONTEXT {
    ULONG ConnectionSpeed;
    USHORT Address;
};

struct BCM_I2C_INTERRUPT_CONTEXT {
    struct WRITE_CONTEXT {
        const BYTE* WriteBufferPtr;
        const BYTE* CurrentWriteBufferPtr;
        const BYTE* EndPtr;
    };

    struct READ_CONTEXT {
        const BYTE* ReadBufferPtr;
        BYTE* CurrentReadBufferPtr;
        const BYTE* EndPtr;
    };

    struct SEQUENCE_CONTEXT {
        PMDL CurrentWriteMdl;
        ULONG BytesToWrite;
        ULONG BytesWritten;
        ULONG CurrentWriteMdlOffset;

        PMDL CurrentReadMdl;
        ULONG BytesToRead;
        ULONG BytesRead;
        ULONG CurrentReadMdlOffset;
    };

    BCM_I2C_REGISTERS* RegistersPtr;
    ULONG State;    // TRANSFER_STATE
    SPBREQUEST SpbRequest;
    ULONG CapturedStatus;
    ULONG CapturedDataLength;
    KSPIN_LOCK CancelLock;
    const BCM_I2C_TARGET_CONTEXT* TargetPtr;
    WDFINTERRUPT WdfInterrupt;

    union {
        WRITE_CONTEXT WriteContext;
        READ_CONTEXT ReadContext;
        SEQUENCE_CONTEXT SequenceContext;
    };
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    BCM_I2C_DEVICE_CONTEXT,
    GetDeviceContext);

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    BCM_I2C_TARGET_CONTEXT,
    GetTargetContext);

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    BCM_I2C_INTERRUPT_CONTEXT,
    GetInterruptContext);

// NONPAGED
EVT_SPB_CONTROLLER_READ OnRead;
EVT_SPB_CONTROLLER_WRITE OnWrite;
EVT_SPB_CONTROLLER_SEQUENCE OnSequence;
EVT_WDF_REQUEST_CANCEL OnRequestCancel;
EVT_WDF_INTERRUPT_ISR OnInterruptIsr;
EVT_WDF_INTERRUPT_DPC OnInterruptDpc;

// PAGED
EVT_SPB_TARGET_CONNECT OnTargetConnect;
EVT_WDF_WORKITEM EvtSampleStatusWorkItem;

EVT_WDF_DEVICE_D0_ENTRY OnD0Entry;
EVT_WDF_DEVICE_D0_EXIT OnD0Exit;
EVT_WDF_DEVICE_PREPARE_HARDWARE OnPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE OnReleaseHardware;

#endif // _DEVICE_H_
