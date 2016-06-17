//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011interrupt.cpp
//
// Abstract:
//
//    This module contains methods and callback implementation
//    for handling ARM PL011 UART interrupts.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//
#include "precomp.h"
#pragma hdrstop

#define _PL011_INTERRUPT_CPP_

// Logging header files
#include "PL011logging.h"
#include "PL011interrupt.tmh"

// Common driver header files
#include "PL011common.h"

// Module specific header files
#include "PL011interrupt.h"
#include "PL011rx.h"
#include "PL011tx.h"


//
// Routine Description:
//
//  PL011EvtInterruptIsr is the interrupt service routine for 
//  ARM PL011 UART. PL011EvtInterruptIsr calls PL011pInterruptIsr to do
//  incoming events ISR level processing, and schedules a DPC level 
//  processing PL011EvtInterruptDpc if our device generated the interrupt.
//
// Arguments:
//
//  WdfInterrupt - The WDF interrupt object the created for the ARM PL011
//      interrupt.
//
//  MessageID - The message ID, in case the device is using MSIs, otherwise 0.
//
// Return Value:
//
//  TRUE if the interrupt originated from the PL011 device and was serviced,
//  otherwise FALSE.
//
_Use_decl_annotations_
BOOLEAN
PL011EvtInterruptIsr(
    WDFINTERRUPT WdfInterrupt,
    ULONG MessageID
    )
{
    UNREFERENCED_PARAMETER(MessageID);

    WDFDEVICE wdfDevice = WdfInterruptGetDevice(WdfInterrupt);
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(wdfDevice);

    //
    // Get and process UART events (ISR level)
    //
    BOOLEAN isUartInterrupt = PL011pInterruptIsr(devExtPtr);
    if (!isUartInterrupt) {

        return FALSE;
    }

    PL011_ASSERT(WdfInterrupt == devExtPtr->WdfUartInterrupt);

    WdfInterruptQueueDpcForIsr(devExtPtr->WdfUartInterrupt);

    PL011_LOG_TRACE(
        "UART ISR, status 0x%04X",
        USHORT(devExtPtr->IntEventsForDpc)
        );

    return TRUE;
}


//
// Routine Description:
//
//  PL011EvtInterruptDpc is the called by the framework, when 
//  the interrupt processing requires more work that needs to be done
//  at IRQL lower than ISR.
//  The routine processes each received events:
//  1) RX interrupt:
//     - Copy from RX FIFO
//     - Indicate new data to SerCx2, if RX state allows it.
//  2) TX interrupt:
//     - Copy to TX FIFO
//     - Notify SerCx2 of TX data availability if TX state allows it.
//  3) Serial port events:
//      - Notify SerCx2 if it was waiting for those.
//
// Arguments:
//
//  WdfInterrupt - The WDF interrupt object the created for the ARM PL011
//      interrupt.
//
//  AssociatedObject - Our WDFDEVICE object.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011EvtInterruptDpc(
    WDFINTERRUPT WdfInterrupt,
    WDFOBJECT AssociatedObject
    )
{
    UNREFERENCED_PARAMETER(AssociatedObject);

    WDFDEVICE wdfDevice = WdfInterruptGetDevice(WdfInterrupt);
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(wdfDevice);

    //
    // Get new events ISR added
    //
    ULONG interruptEventsToHandle = ULONG(InterlockedExchange(
            reinterpret_cast<volatile LONG*>(&devExtPtr->IntEventsForDpc),
            0));

    //
    // RX interrupt:
    // If a character has been received, or the FIFO is
    // not empty, and RX timeout has occurred.
    // Basically if RX FIFO is not empty.
    //
    if ((interruptEventsToHandle & (UARTRIS_RXIS | UARTRIS_RTIS)) != 0) {

        PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
            PL011SerCxPioReceiveGetContext(devExtPtr->SerCx2PioReceive);

        //
        // Copy new data from RX FIFO to PIO RX buffer.
        //
        (void)PL011RxPioFifoCopy(devExtPtr, nullptr);

        //
        // Notify SerCxs if we have new data, notifications have 
        // not been canceled, and SerCx2 was not already notified.
        //
        if (PL011RxPendingByteCount(rxPioPtr) > 0) {

            if (PL011RxPioStateSetCompare(
                    devExtPtr->SerCx2PioReceive,
                    PL011_RX_PIO_STATE::RX_PIO_STATE__WAIT_READ_DATA,
                    PL011_RX_PIO_STATE::RX_PIO_STATE__WAIT_DATA
                    )) {
                //
                // RX Data is ready, come get it...
                //
                SerCx2PioReceiveReady(devExtPtr->SerCx2PioReceive);

            } // If RX state set to RX_PIO_STATE__WAIT_READ_DATA

        } // New RX data is ready

    } // if (RX interrupt)

    //
    // TX interrupt:
    // If TX FIFOs occupancy has gone bellow the configured
    // trigger level.
    //
    if ((interruptEventsToHandle & UARTRIS_TXIS) != 0) {

        PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
            PL011SerCxPioTransmitGetContext(devExtPtr->SerCx2PioTransmit);

        //
        // Copy pending data from PIO TX buffer to TX FIFO, if any...
        //
        (void)PL011TxPioFifoCopy(devExtPtr, nullptr);

        //
        // Notify SerCxs if more space is now available, 
        // notifications have not been canceled, and SerCx2
        // was not already notified.
        //
        if (PL011TxPendingByteCount(txPioPtr) < PL011_TX_BUFFER_SIZE_BYTES) {

            if (PL011TxPioStateSetCompare(
                    devExtPtr->SerCx2PioTransmit,
                    PL011_TX_PIO_STATE::TX_PIO_STATE__WAIT_SEND_DATA,
                    PL011_TX_PIO_STATE::TX_PIO_STATE__WAIT_DATA_SENT
                    )) {
                //
                // Ready for more TX data...
                //
                SerCx2PioTransmitReady(devExtPtr->SerCx2PioTransmit);

            } // if TX state set TX_PIO_STATE__WAIT_SEND_DATA

        } // TX space is available

    } // if (TX interrupt)

    //
    // Record errors and break events, if any...
    //
    PL011DeviceRecordErrors(
        devExtPtr, 
        (interruptEventsToHandle & (UART_INTERUPPTS_ERRORS | UARTRIS_BEIS))
        );

    //
    // Notify the framework of new events, if any...
    //
    PL011DeviceNotifyEvents(
        devExtPtr,
        interruptEventsToHandle
        );
}


//
// Routine Description:
//
//  PL011pInterruptIsr is called by PL011EvtInterruptIsr 
//  to process interrupts events at ISR level.
//  The routine saves the received event mask in the device extension
//  for DPC processing.
//  If RX/TX FIFO events occur, the routine calls the RX/TX handlers
//  to copy new/pending data from/to RX/TX FIFOs.
//
// Arguments:
//
//  DevExtPtr - Our device extension.
//
// Return Value:
//
//  TRUE If interrupt has been serviced, i.e. the source was the PL011 UART,
//  otherwise FALSE.
//
_Use_decl_annotations_
BOOLEAN
PL011pInterruptIsr(
    PL011_DEVICE_EXTENSION* DevExtPtr
    )
{
    //
    // Interrupt status register
    //
    ULONG regUARTRIS = PL011HwReadRegisterUlong(
        PL011HwRegAddress(DevExtPtr, UARTRIS)
        );
    if ((regUARTRIS & UART_INTERUPPTS_ALL) == 0) {
        //
        // Not the UART interrupt
        //
        return FALSE;
    }

    //
    // Update the events mask to be handled at DPC 
    // level.
    //
    InterlockedOr(
        reinterpret_cast<volatile LONG*>(&DevExtPtr->IntEventsForDpc),
        regUARTRIS
        );

    //
    // RX interrupt:
    // If a character has been received, or the FIFO is
    // not empty, and RX timeout has occurred.
    // Basically if RX FIFO is not empty.
    //
    if ((regUARTRIS & (UARTRIS_RXIS | UARTRIS_RTIS)) != 0) {
        //
        // Copy new data from RX FIFO to PIO RX buffer.
        //
        (void)PL011RxPioFifoCopy(DevExtPtr, nullptr);

        //
        // Update the state to RX_PIO_STATE__DATA_READY if
        // we are still reading data to let the read engine 
        // know new data is ready.
        //
        (void)PL011RxPioStateSetCompare(
            DevExtPtr->SerCx2PioReceive,
            PL011_RX_PIO_STATE::RX_PIO_STATE__DATA_READY,
            PL011_RX_PIO_STATE::RX_PIO_STATE__READ_DATA
            );

    } // if (RX interrupt)

    //
    // TX interrupt:
    // If TX FIFOs occupancy has gone bellow the configured
    // trigger level.
    //
    if ((regUARTRIS & UARTRIS_TXIS) != 0) {
        //
        // Copy pending data from PIO TX buffer to TX FIFO.
        //
        (void)PL011TxPioFifoCopy(DevExtPtr, nullptr);

        (void)PL011TxPioStateSetCompare(
            DevExtPtr->SerCx2PioTransmit,
            PL011_TX_PIO_STATE::TX_PIO_STATE__DATA_SENT,
            PL011_TX_PIO_STATE::TX_PIO_STATE__SEND_DATA
            );

    } // if (TX interrupt)

    //
    // Acknowledge the events we just processed.
    //
    PL011HwWriteRegisterUlong(
        PL011HwRegAddress(DevExtPtr, UARTICR),
        regUARTRIS
        );

    return TRUE;
}


#undef _PL011_INTERRUPT_CPP_