//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011uart.h
//
// Abstract:
//
//    This module contains common enum, types, and methods definitions
//    for SerCx2 general event callbacks implemented by the ARM PL011 
//    device driver.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//

#ifndef _PL011_UART_H_
#define _PL011_UART_H_

WDF_EXTERN_C_START


//
// The UART serial bus type code in ACPI connection
// information.
//
#define UART_SERIAL_BUS_TYPE                0x03
//
// The UART serial bus codes in ACPI connection
// information (TypeSpecificFlags)
//
#define UART_SERIAL_BUS_TYPE                0x03
#define UART_SERIAL_FLAG_FLOW_CTL_NONE      (0x0000 << 0)
#define UART_SERIAL_FLAG_FLOW_CTL_HW        (0x0001 << 0)
#define UART_SERIAL_FLAG_FLOW_CTL_XONXOFF   (0x0002 << 0)
#define UART_SERIAL_FLAG_FLOW_CTL_MASK      (0x0003 << 0)
#define UART_SERIAL_FLAG_STOP_BITS_0        (0x0000 << 2)
#define UART_SERIAL_FLAG_STOP_BITS_1        (0x0001 << 2)
#define UART_SERIAL_FLAG_STOP_BITS_1_5      (0x0002 << 2)
#define UART_SERIAL_FLAG_STOP_BITS_2        (0x0003 << 2)
#define UART_SERIAL_FLAG_STOP_BITS_MASK     (0x0003 << 2)
#define UART_SERIAL_FLAG_DATA_BITS_5        (0x0000 << 4)
#define UART_SERIAL_FLAG_DATA_BITS_6        (0x0001 << 4)
#define UART_SERIAL_FLAG_DATA_BITS_7        (0x0002 << 4)
#define UART_SERIAL_FLAG_DATA_BITS_8        (0x0003 << 4)
#define UART_SERIAL_FLAG_DATA_BITS_9        (0x0004 << 4)
#define UART_SERIAL_FLAG_DATA_BITS_MASK     (0x0007 << 4)
#define UART_SERIAL_FLAG_BIG_ENDIAN         (0x0001 << 7)
#define UART_SERIAL_PARITY_NONE             0x00
#define UART_SERIAL_PARITY_EVEN             0x01
#define UART_SERIAL_PARITY_ODD              0x02
#define UART_SERIAL_PARITY_MARK             0x03
#define UART_SERIAL_PARITY_SPACE            0x04
//
// The UART serial bus codes in ACPI connection
// information (SerialLinesEnabled)
//
#define UART_SERIAL_LINES_DCD               (0x0001 << 2)
#define UART_SERIAL_LINES_RI                (0x0001 << 3)
#define UART_SERIAL_LINES_DSR               (0x0001 << 4)
#define UART_SERIAL_LINES_DTR               (0x0001 << 5)
#define UART_SERIAL_LINES_CTS               (0x0001 << 6)
#define UART_SERIAL_LINES_RTS               (0x0001 << 7)
//
// PL011 specific
//
#define UART_SERIAL_LINES_OUT1              (0x0001 << 8)
#define UART_SERIAL_LINES_OUT2              (0x0001 << 9)


//
// These masks define access the modem control register (MCR)
//

//
// This bit controls the data terminal ready (DTR) line.  When
// this bit is set the line goes to logic 0 (which is then inverted
// by normal hardware).  This is normally used to indicate that
// the device is available to be used.  Some odd hardware
// protocols (like the kernel debugger) use this for handshaking
// purposes.
//
#define SERIAL_MCR_DTR            0x01

//
// This bit controls the ready to send (RTS) line.  When this bit
// is set the line goes to logic 0 (which is then inverted by the normal
// hardware).  This is used for hardware handshaking.  It indicates that
// the hardware is ready to send data and it is waiting for the
// receiving end to set clear to send (CTS).
//
#define SERIAL_MCR_RTS            0x02

//
// This bit is used for general purpose output.
//
#define SERIAL_MCR_OUT1           0x04

//
// This bit is used for general purpose output.
//
#define SERIAL_MCR_OUT2           0x08

//
// This bit controls the loop-back testing mode of the device.  Basically
// the outputs are connected to the inputs (and vice versa).
//
#define SERIAL_MCR_LOOP           0x10

//
// This bit enables auto flow control on a TI TL16C550C/TL16C550CI
//
#define SERIAL_MCR_TL16C550CAFE   0x20

//
// Enable automatic hardware flow control, managed by the device.
// If RTS_EN is set, RTS will be removed when the receive FIFO is
// at the trigger level.  If CTS_EN is set, the UART will stop
// transmitting when CTS is taken away.
//
#define SERIAL_MCR_CTS_EN         0x20
#define SERIAL_MCR_RTS_EN         0x40


//
// These masks define access the FIFO control register (FCR)
//

//
// Enabling this bit in the FIFO control register will turn
// on the FIFOs.  If the FIFO are enabled then the high two
// bits of the interrupt id register will be set to one.  Note
// that this only occurs on a 16550 class chip.  If the high
// two bits in the interrupt id register are not one then
// we know we have a lower model chip.
//
//
#define SERIAL_FCR_ENABLE     ((UCHAR)0x01)
#define SERIAL_FCR_RCVR_RESET ((UCHAR)0x02)
#define SERIAL_FCR_TXMT_RESET ((UCHAR)0x04)

//
// The mode of the DMA, setting to 0 will make Rx DMA trigger
// based on if there is content in Rx FIFO.  This will result
// in single byte transactions and is not recommended.
// Setting to 1 will make Rx DMA trigger base on high water
// mark or Rx timeout and is recommended.
//
#define SERIAL_FCR_DMA_MODE   ((UCHAR)0x08)

//
// This set of values define the high water marks (when the
// interrupts trip) for the receive FIFO.
//
#define SERIAL_RX_FIFO_MASK        ((UCHAR)0xc0)
#define SERIAL_1_BYTE_HIGH_WATER   ((UCHAR)0x00)
#define SERIAL_4_BYTE_HIGH_WATER   ((UCHAR)0x40)
#define SERIAL_8_BYTE_HIGH_WATER   ((UCHAR)0x80)
#define SERIAL_14_BYTE_HIGH_WATER  ((UCHAR)0xc0)

//
// This set of values define the TX trigger for the transfer
// FIFO.  For the transfer FIFO to be considered empty, it must
// have at least the corresponding amount of free bytes below.
//
#define SERIAL_TX_FIFO_MASK     ((UCHAR)(3 << 4))
#define SERIAL_TX_1_BYTE_TRIG   ((UCHAR)(0 << 4))
#define SERIAL_TX_4_BYTE_TRIG   ((UCHAR)(1 << 4))
#define SERIAL_TX_8_BYTE_TRIG   ((UCHAR)(2 << 4))
#define SERIAL_TX_14_BYTE_TRIG  ((UCHAR)(3 << 4))


//
// Serial peripheral bus descriptor
//

#include "pshpack1.h"

//
// PNP_UART_SERIAL_BUS_DESCRIPTOR
// Provided through the framework 
//
typedef struct _PNP_UART_SERIAL_BUS_DESCRIPTOR {
    PNP_SERIAL_BUS_DESCRIPTOR   SerialBusDescriptor;
    ULONG                       BaudRate;
    USHORT                      RxBufferSize;
    USHORT                      TxBufferSize;
    UCHAR                       Parity;
    UCHAR                       SerialLinesEnabled;
    //
    // followed by optional Vendor Data
    // followed by resource name string
    //
} PNP_UART_SERIAL_BUS_DESCRIPTOR, *PPNP_UART_SERIAL_BUS_DESCRIPTOR;

//
// PL011_UART_SERIAL_BUS_DESCRIPTOR
// An extension of PNP_UART_SERIAL_BUS_DESCRIPTOR with
// PL011 specific information.
//
typedef struct _PL011_UART_SERIAL_BUS_DESCRIPTOR {
    PNP_UART_SERIAL_BUS_DESCRIPTOR  UartSerialBusDescriptor;
    //
    // PL011 specific
    //
    ULONG                           MaxBaudRateBPS;
    ULONG                           UartClockHz;
    SERIAL_HANDFLOW                 FlowControlSetup;
    SERIAL_LINE_CONTROL             LineControlSetup;
} PL011_UART_SERIAL_BUS_DESCRIPTOR, *PPL011_UART_SERIAL_BUS_DESCRIPTOR;

#include "poppack.h"


//
// PL011uart SerCX2 device general event handlers
// More SerCX2 event handlers reside in the following modules:
// - PL011rx
// - PL011tx
// - PL011interrupt
//
EVT_SERCX2_FILEOPEN PL011EvtSerCx2FileOpen;
EVT_SERCX2_FILECLOSE PL011EvtSerCx2FileClose;
EVT_SERCX2_APPLY_CONFIG PL011EvtSerCx2ApplyConfig;
EVT_SERCX2_CONTROL PL011EvtSerCx2Control;
EVT_SERCX2_PURGE_FIFOS PL011EvtSerCx2PurgeFifos;
EVT_SERCX2_SET_WAIT_MASK PL011EvtSerCx2SetWaitMask;


//
// PL011uart public methods
//

//
// PL011rx private methods
//
#ifdef _PL011_UART_CPP_

    _IRQL_requires_max_(PASSIVE_LEVEL)
    static NTSTATUS
    PL011pParseSerialBusDescriptor(
        _In_ WDFDEVICE WdfDevice,
        _In_ VOID* ConnectionParametersPtr,
        _Out_ PNP_UART_SERIAL_BUS_DESCRIPTOR** UartDescriptorPPtr
        );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    static NTSTATUS
    PL011pApplyConfig(
        _In_ WDFDEVICE WdfDevice,
        _In_ PNP_UART_SERIAL_BUS_DESCRIPTOR* PnpUartDescriptorPtr
        );

#endif //_PL011_UART_CPP_


WDF_EXTERN_C_END

#endif // !_PL011_UART_H_
