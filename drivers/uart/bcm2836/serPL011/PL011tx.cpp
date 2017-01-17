//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011tx.h
//
// Abstract:
//
//    This module contains implementation of methods and callbacks
//    associated with the ARM PL011 UART device driver transmit process.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//
#include "precomp.h"
#pragma hdrstop

#define _PL011_TX_CPP_

// Logging header files
#include "PL011logging.h"
#include "PL011tx.tmh"

// Common driver header files
#include "PL011common.h"

// Module specific header files
#include "PL011tx.h"


#ifdef ALLOC_PRAGMA
    #pragma alloc_text(PAGE, PL011TxPioTransmitInit)
#endif // ALLOC_PRAGMA


//
// TX PIO state strings
//
const char* TxPioStateStr[] = {
    "PIO TX", // First entry is the preamble 
    "TX_PIO_STATE__OFF",
    "TX_PIO_STATE__IDLE",
    "TX_PIO_STATE__SEND_DATA",
    "TX_PIO_STATE__WAIT_DATA_SENT",
    "TX_PIO_STATE__DATA_SENT",
    "TX_PIO_STATE__WAIT_SEND_DATA",
    "TX_PIO_STATE__DRAIN_FIFO",
    "TX_PIO_STATE__PURGE_FIFO",
};
SIZE_T TxPioStateLength = ARRAYSIZE(TxPioStateStr);


//
// Routine Description:
//
//  PL011TxPioTransmitInit is called by PL011pDeviceSerCx2Init to
//  initialize the TX PIO transaction context.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
// Return Value:
//
//  Initialization status code
//
_Use_decl_annotations_
NTSTATUS
PL011TxPioTransmitInit(
    WDFDEVICE WdfDevice,
    SERCX2PIOTRANSMIT SerCx2PioTransmit
    )
{
    PAGED_CODE();

    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(SerCx2PioTransmit);

    RtlZeroMemory(txPioPtr, sizeof(PL011_SERCXPIOTRANSMIT_CONTEXT));

    txPioPtr->DevExtPtr = devExtPtr;
    txPioPtr->TxPioState = PL011_TX_PIO_STATE::TX_PIO_STATE__OFF;

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011TxPioTransamitStart is called on first device open to start
//  TX process.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
// Return Value:
//
//  STATUS_SUCCESS
//
_Use_decl_annotations_
NTSTATUS
PL011TxPioTransmitStart(
    WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(devExtPtr->SerCx2PioTransmit);

    (void)PL011TxPioStateSet(
        devExtPtr->SerCx2PioTransmit,
        PL011_TX_PIO_STATE::TX_PIO_STATE__OFF
        );

    txPioPtr->TxBufferIn = 0;
    txPioPtr->TxBufferOut = 0;
    txPioPtr->TxBufferCount = 0;

    //
    // Disable TX interrupt
    //
    PL011HwMaskInterrupts(
        WdfDevice,
        UARTIMSC_TXIM,
        TRUE, // Mask
        TRUE // Not ISR code
        );

    //
    // Enable TX
    //
    PL011HwUartControl(
        WdfDevice,
        UARTCR_TXE,
        REG_UPDATE_MODE::BITMASK_SET,
        nullptr
        );

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011TxPioTransmitStop is called on last device close to stop
//  TX process.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
// Return Value:
//
//  STATUS_SUCCESS
//
_Use_decl_annotations_
NTSTATUS
PL011TxPioTransmitStop(
    WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(devExtPtr->SerCx2PioTransmit);

    (void)PL011TxPioStateSet(
        devExtPtr->SerCx2PioTransmit,
        PL011_TX_PIO_STATE::TX_PIO_STATE__OFF
        );

    RtlZeroMemory(txPioPtr->TxBuffer, sizeof(txPioPtr->TxBuffer));

    //
    // Disable TX interrupt
    //
    PL011HwMaskInterrupts(
        WdfDevice,
        UARTIMSC_TXIM,
        TRUE, // Mask
        TRUE // Not ISR code
        );

    //
    // Disable TX
    //
    PL011HwUartControl(
        WdfDevice,
        UARTCR_TXE,
        REG_UPDATE_MODE::BITMASK_CLEAR,
        nullptr
        );

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011SerCx2EvtPioTransmitWriteBuffer is called by SerCx2 framework to use 
//  programmed IO (PIO) to transfer data from the caller buffer to
//  the transmit FIFO.
//
// Arguments:
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
//  BufferPtr - The caller buffer from which FIFO bytes should be
//      transferred.
//
//  Length - Size in bytes of the caller buffer.
//
// Return Value:
//
//  Number of bytes successfully written to TX FIFO.
//
_Use_decl_annotations_
ULONG
PL011SerCx2EvtPioTransmitWriteBuffer(
    SERCX2PIOTRANSMIT SerCx2PioTransmit,
    UCHAR* BufferPtr,
    ULONG Length
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(SerCx2PioTransmit);
    PL011_DEVICE_EXTENSION* devExtPtr = txPioPtr->DevExtPtr;

    (void)PL011TxPioStateSet(
            SerCx2PioTransmit, 
            PL011_TX_PIO_STATE::TX_PIO_STATE__SEND_DATA
            );

    //
    // Copy TX data from caller's buffer to
    // TX buffer -> TX FIFO.
    //
    ULONG totalBytesCopied = 0;
    while (totalBytesCopied < Length) {
        //
        // Caller buffer -> TX buffer
        //
        totalBytesCopied += PL011pTxPioBufferCopy(
            devExtPtr,
            BufferPtr + totalBytesCopied,
            Length - totalBytesCopied
            );

        //
        // TX buffer -> TX FIFO
        //
        NTSTATUS status = PL011TxPioFifoCopy(devExtPtr, nullptr);
        if (status == STATUS_NO_MORE_FILES) {
            // 
            // TX FIFO is full...
            //
            break;
        }

    } // Copy TX chars to caller buffer

    PL011_LOG_TRACE(
        "PIO TX: written %lu chars", totalBytesCopied
        );

    PL011_ASSERT(totalBytesCopied <= Length);

    return totalBytesCopied;
}


//
// Routine Description:
//
//  PL011SerCx2EvtPioTransmitEnableReadyNotification is called by SerCx2 
//  to enable the driver to notify SerCx2 when the serial controller 
//  receives new data through SerCx2PioTransmitReady.
//
// Arguments:
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011SerCx2EvtPioTransmitEnableReadyNotification(
    SERCX2PIOTRANSMIT SerCx2PioTransmit
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(SerCx2PioTransmit);
    PL011_DEVICE_EXTENSION* devExtPtr = txPioPtr->DevExtPtr;

    (void)PL011TxPioStateSet(
        SerCx2PioTransmit,
        PL011_TX_PIO_STATE::TX_PIO_STATE__SEND_DATA
        );

    //
    // Check if we are ready for more data
    //
    if (PL011TxPendingByteCount(txPioPtr) < PL011_TX_BUFFER_SIZE_BYTES) {

        SerCx2PioTransmitReady(SerCx2PioTransmit);
        return;
    }

    //
    // Mark that we are waiting for TX FIFO to become not full, so we can
    // send more data.
    // The driver will not call SerCx2PioTransmitReady unless 
    // PIO RX state is TX_PIO_STATE__WAIT_DATA_SENT.
    //
    if (!PL011TxPioStateSetCompare(
            SerCx2PioTransmit,
            PL011_TX_PIO_STATE::TX_PIO_STATE__WAIT_DATA_SENT,
            PL011_TX_PIO_STATE::TX_PIO_STATE__SEND_DATA
            )) {
        //
        // More TX data can already be sent...
        //
        PL011_ASSERT(
            PL011TxPioStateGet(SerCx2PioTransmit) ==
            PL011_TX_PIO_STATE::TX_PIO_STATE__DATA_SENT
            );
        PL011_ASSERT(
            PL011TxPendingByteCount(txPioPtr) < PL011_TX_BUFFER_SIZE_BYTES
            );

        SerCx2PioTransmitReady(SerCx2PioTransmit);
        return;
    }

    //
    // Enable TX interrupt
    //
    PL011HwMaskInterrupts(
        devExtPtr->WdfDevice,
        UARTIMSC_TXIM,
        FALSE, // unmask
        TRUE // ISR safe
        );
}


//
// Routine Description:
//
//  PL011SerCx2EvtPioReceiveCancelReadyNotification is called by SerCx2 
//  to cancel a previous call to
//  PL011SerCx2EvtPioTransmitEnableReadyNotification.
//
// Arguments:
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
// Return Value:
//
//  TRUE - 'Ready Notifications' were successfully disabled, or FALSE if
//      SerCx2PioTransmitReady was called or is about to be called. 
//
_Use_decl_annotations_
BOOLEAN
PL011SerCx2EvtPioTransmitCancelReadyNotification(
    SERCX2PIOTRANSMIT SerCx2PioTransmit
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(SerCx2PioTransmit);
    PL011_DEVICE_EXTENSION* devExtPtr = txPioPtr->DevExtPtr;

    // Reset the TX PIO state 
    PL011_TX_PIO_STATE prevTxPioState = PL011TxPioStateSet(
        SerCx2PioTransmit,
        PL011_TX_PIO_STATE::TX_PIO_STATE__IDLE
        );

    //
    // Disable TX interrupt
    //
    PL011HwMaskInterrupts(
        devExtPtr->WdfDevice,
        UARTIMSC_TXIM,
        TRUE, // mask
        TRUE // ISR safe
        );

    BOOLEAN isCanceled = 
        prevTxPioState != PL011_TX_PIO_STATE::TX_PIO_STATE__WAIT_SEND_DATA;

    PL011_LOG_TRACE(
        "PIO TX cancel notifications: -> %lu", isCanceled
        );

    return isCanceled;
}


//
// Routine Description:
//
//  PL011SerCx2EvtPioTransmitDrainFifo is called by SerCx2 
//  to drain the TX FIFO.
//  Draining the TX FIFO is an async operation, i.e. when all data 
//  in TX FIFO is sent, the drivers calls SerCx2PioTransmitDrainFifoComplete.
//
//  The routine copies pending TX chars to TX FIFO, and leaves. When
//  TX FIFO is empty ISR/DPC will continue to feed TX FIFO until all 
//  TX buffer becomes empty. 
//
// Arguments:
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011SerCx2EvtPioTransmitDrainFifo(
    SERCX2PIOTRANSMIT SerCx2PioTransmit
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(SerCx2PioTransmit);
    PL011_DEVICE_EXTENSION* devExtPtr = txPioPtr->DevExtPtr;

    PL011_LOG_TRACE(
        "PIO drain TX FIFO"
        );

    (void)PL011TxPioStateSet(
        SerCx2PioTransmit,
        PL011_TX_PIO_STATE::TX_PIO_STATE__DRAIN_FIFO
        );

    //
    // Disable TX interrupt
    //
    PL011HwMaskInterrupts(
        devExtPtr->WdfDevice,
        UARTIMSC_TXIM,
        TRUE, // mask
        TRUE // ISR safe
        );

    //
    // Drain TX buffer/FIFO
    //
    do {

        (void)PL011TxPioFifoCopy(devExtPtr, nullptr);

    } while ((PL011TxPendingByteCount(txPioPtr) != 0) || 
        PL011HwIsTxBusy(devExtPtr));

    //
    // Make sure 'Drain FiFo' was not canceled...
    //
    PL011_TX_PIO_STATE prevTxPioState = PL011TxPioStateSet(
        SerCx2PioTransmit,
        PL011_TX_PIO_STATE::TX_PIO_STATE__IDLE
        );
    if (prevTxPioState == PL011_TX_PIO_STATE::TX_PIO_STATE__DRAIN_FIFO) {

        SerCx2PioTransmitDrainFifoComplete(devExtPtr->SerCx2PioTransmit);
    }

    PL011_LOG_TRACE(
        "PIO drain TX FIFO Done! Previous state %d",
        ULONG(prevTxPioState)
        );
}


//
// Routine Description:
//
//  PL011SerCx2EvtPioTransmitCancelDrainFifo is called by SerCx2 
//  to cancel a previous 'Drain TX FIFO' request.
//
// Arguments:
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
// Return Value:
//
//  TRUE - The 'Drain TX FIFO' was successfully canceled 
//  (SerCx2PioTransmitDrainFifoComplete will not be called), otherwise FALSE.
//
_Use_decl_annotations_
BOOLEAN
PL011SerCx2EvtPioTransmitCancelDrainFifo(
    SERCX2PIOTRANSMIT SerCx2PioTransmit
    )
{
    PL011_LOG_INFORMATION(
        "PIO cancel drain TX FIFO"
        );
    
    //
    // Reset the TX PIO state 
    //
    PL011_TX_PIO_STATE prevTxPioState = PL011TxPioStateSet(
        SerCx2PioTransmit,
        PL011_TX_PIO_STATE::TX_PIO_STATE__IDLE
        );

    BOOLEAN isCanceled = 
        prevTxPioState == PL011_TX_PIO_STATE::TX_PIO_STATE__DRAIN_FIFO;

    PL011_LOG_INFORMATION(
        "PIO TX cancel drain FIFO: -> %d", isCanceled
        );

    return isCanceled;
}


//
// Routine Description:
//
//  PL011SerCx2EvtPioTransmitPurgeFifo is called by SerCx2 
//  to discard any bytes in 'TX FIFO' request.
//
//  The routine discards all pending TX chars, and calls
//  SerCx2PioTransmitPurgeFifoComplete.
//
// Arguments:
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
//  BytesAlreadyTransmittedToHardware - The number of bytes that 
//      have already been loaded into the transmit FIFO.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011SerCx2EvtPioTransmitPurgeFifo(
    SERCX2PIOTRANSMIT SerCx2PioTransmit,
    ULONG BytesAlreadyTransmittedToHardware
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(SerCx2PioTransmit);
    PL011_DEVICE_EXTENSION* devExtPtr = txPioPtr->DevExtPtr;

    UNREFERENCED_PARAMETER(BytesAlreadyTransmittedToHardware);

    PL011_LOG_INFORMATION(
        "PIO TX purge FIFO!"
        );

    //
    // Disable TX interrupt
    //
    PL011HwMaskInterrupts(
        devExtPtr->WdfDevice,
        UARTIMSC_TXIM,
        TRUE, // mask
        TRUE  // ISR safe
        );

    ULONG purgedBytes;
    PL011pTxPioPurgeFifo(txPioPtr->DevExtPtr->WdfDevice, &purgedBytes);

    SerCx2PioTransmitPurgeFifoComplete(SerCx2PioTransmit, purgedBytes);

    PL011_LOG_INFORMATION(
        "PIO TX purge FIFO Done!"
        );
}


//
// Routine Description:
//
//  PL011TxPurgeFifo is called by either PL011SerCx2EvtPioTransmitPurgeFifo 
//  or PL011EvtSerCx2PurgeFifos to purge the TX FIFO either PIO or DMA.
//
//  The routine discards all pending TX chars, and calls
//  SerCx2PioTransmitPurgeFifoComplete.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  PurgedBytesPtr - Address of a caller ULONG var to receive the number of
//      bytes purged, or nullptr if not needed.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011TxPurgeFifo(
    WDFDEVICE WdfDevice,
    ULONG* PurgedBytesPtr
    )
{
    //
    // Purge PIO TX buffer/FIFO
    //
    PL011pTxPioPurgeFifo(WdfDevice, PurgedBytesPtr);
}


//
// Routine Description:
//
//  PL011TxPioFifoCopy is called to copy new outgoing data from TX buffer 
//  to TX FIFO.
//  The routine can be called from PL011SerCx2EvtPioTransmitWriteBuffer
//  or TX interrupt.
//
// Arguments:
//
//  DevExtPtr - Our device context.
//
//  CharsCopiedPtr - Address of a caller ULONG var to received the number
//      of characters copied, or nullptr if not required.
//
// Return Value:
//
//  STATUS_SUCCESS - Data was successfully copied from TX buffer
//      TX FIFO.
//  STATUS_NO_MORE_FILES - TX BUFFER and TX FIFO are full, no chars were copied.
//  STATUS_DEVICE_BUSY - A previous call to  PL011TxFifoCopy is currently in
//      progress.
//
_Use_decl_annotations_
NTSTATUS
PL011TxPioFifoCopy(
    PL011_DEVICE_EXTENSION* DevExtPtr,
    ULONG* CharsCopiedPtr
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(DevExtPtr->SerCx2PioTransmit);

    if (CharsCopiedPtr != nullptr) {

        *CharsCopiedPtr = 0;
    }

    //
    // Sync access to TX FIFO
    //
    if (InterlockedExchange(&txPioPtr->TxBufferLock, 1) != 0) {

        return STATUS_DEVICE_BUSY;
    }

    //
    // Used register addresses
    //
    // Data register address
    volatile ULONG* regUARTDRPtr = PL011HwRegAddress(DevExtPtr, UARTDR);
    // Flags register address
    volatile ULONG* regUARTFRPtr = PL011HwRegAddress(DevExtPtr, UARTFR);

    //
    // Write pending chars from TX buffer to TX FIFO
    //
    NTSTATUS status = STATUS_SUCCESS;
    ULONG charsTransferred = 0;
    ULONG txOut = txPioPtr->TxBufferOut;
    volatile LONG* txPendingCountPtr = &txPioPtr->TxBufferCount;

    while (PL011TxPendingByteCount(txPioPtr) > 0) {
        //
        // Check if TX FIFO is full
        //
        if ((PL011HwReadRegisterUlong(regUARTFRPtr) & UARTFR_TXFF) != 0) {

            if ((charsTransferred == 0) &&
                (PL011TxPendingByteCount(txPioPtr) == 
                 PL011_TX_BUFFER_SIZE_BYTES)) {

                status = STATUS_NO_MORE_FILES;

            } else {

                status = STATUS_SUCCESS;
            }
            break;
        }

        //
        // Write next pending word to TX FIFO
        //
        PL011HwWriteRegisterUlongNoFence(
            regUARTDRPtr,
            ULONG(txPioPtr->TxBuffer[txOut])
            );

        ++charsTransferred;
        InterlockedDecrement(txPendingCountPtr);

        txOut = (txOut + 1) % PL011_TX_BUFFER_SIZE_BYTES;

    } // While TX buffer not empty

    txPioPtr->TxBufferOut = txOut;

    if (charsTransferred != 0) {

        PL011_LOG_TRACE(
            "TX FIFO: sent %lu chars, in %lu, out %lu, count %lu",
            charsTransferred,
            txPioPtr->TxBufferIn,
            txPioPtr->TxBufferOut,
            txPioPtr->TxBufferCount
            );
    }

    InterlockedExchange(&txPioPtr->TxBufferLock, 0);

    if (CharsCopiedPtr != nullptr) {

        *CharsCopiedPtr = charsTransferred;
    }

    return status;
}


//
// Routine Description:
//
//  PL011pTxBufferCopy is called to copy new TX data from caller buffer 
//  to the TX buffer.
//
// Arguments:
//
//  DevExtPtr - Our device context.
//
//  BufferPtr - The caller buffer from which TX bytes should be
//      transferred.
//
//  Length - Size in bytes of the caller buffer.
//
// Return Value:
//
//  Number of bytes successfully written to TX buffer.
//
_Use_decl_annotations_
ULONG
PL011pTxPioBufferCopy(
    PL011_DEVICE_EXTENSION* DevExtPtr,
    const UCHAR* BufferPtr,
    ULONG Length
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(DevExtPtr->SerCx2PioTransmit);

    //
    // Get number of bytes we can copy.
    // Is TX Buffer full?
    //
    ULONG bytesToCopy = PL011_TX_BUFFER_SIZE_BYTES - 
        PL011TxPendingByteCount(txPioPtr);
    bytesToCopy = min(bytesToCopy, Length);
    if (bytesToCopy == 0) {
        
        return 0;
    }

    //
    // Copy RX data: Caller buffer -> TX Buffer
    //
    ULONG txIn = txPioPtr->TxBufferIn;
    ULONG bytesCopied = min(bytesToCopy, (PL011_TX_BUFFER_SIZE_BYTES - txIn));

    _Analysis_assume_(bytesCopied <= Length);
    RtlCopyMemory(&txPioPtr->TxBuffer[txIn], BufferPtr, bytesCopied);

    txIn = (txIn + bytesCopied) % PL011_TX_BUFFER_SIZE_BYTES;

    if (bytesCopied < bytesToCopy) {

        BufferPtr += bytesCopied;

        ULONG bytesLeftToCopy = bytesToCopy - bytesCopied;
        RtlCopyMemory(&txPioPtr->TxBuffer, BufferPtr, bytesLeftToCopy);

        bytesCopied += bytesLeftToCopy;
        txIn = bytesLeftToCopy;

    } // if (bytesCopied < bytesToCopy)

    txPioPtr->TxBufferIn = txIn;
    InterlockedAdd(&txPioPtr->TxBufferCount, bytesCopied);

    PL011_ASSERT(PL011TxPendingByteCount(txPioPtr) <= PL011_TX_BUFFER_SIZE_BYTES);

    if (bytesCopied != 0) {

        PL011_LOG_TRACE(
            "TX buffer: written %lu chars, in %lu, out %lu, count %lu",
            bytesCopied,
            txPioPtr->TxBufferIn,
            txPioPtr->TxBufferOut,
            txPioPtr->TxBufferCount
            );
    }

    return bytesCopied;
}


//
// Routine Description:
//
//  PL011pTxPioPurgeFifo is called by either PL011TxPurgeFifo 
//  or PL011EvtSerCx2PurgeFifos to purge the PIO TX FIFO.
//
//  The routine discards all pending TX chars, and calls
//  SerCx2PioTransmitPurgeFifoComplete.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  PurgedBytesPtr - Address of a caller ULONG var to receive the number of
//      bytes purged, or nullptr if not needed.
//
// Return Value:
//
_Use_decl_annotations_
VOID
PL011pTxPioPurgeFifo(
    WDFDEVICE WdfDevice,
    ULONG* PurgedBytesPtr
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(devExtPtr->SerCx2PioTransmit);

    (void)PL011TxPioStateSet(
        devExtPtr->SerCx2PioTransmit,
        PL011_TX_PIO_STATE::TX_PIO_STATE__PURGE_FIFO
        );

    if (InterlockedExchange(&txPioPtr->TxBufferLock, 1) != 0) {
        //
        // We should not get here
        //
        PL011_ASSERT(FALSE);
    }

    //
    // Wait for TX FIFO to drain..
    //
    while (!PL011HwIsTxFifoEmpty(devExtPtr));

    //
    // Reset TX buffer
    //
    ULONG purgedBytes = txPioPtr->TxBufferCount;
    txPioPtr->TxBufferCount = 0;
    txPioPtr->TxBufferIn = 0;
    txPioPtr->TxBufferOut = 0;

    (void)PL011TxPioStateSet(
        devExtPtr->SerCx2PioTransmit,
        PL011_TX_PIO_STATE::TX_PIO_STATE__IDLE
        );

    if (PurgedBytesPtr != nullptr) {

        *PurgedBytesPtr = purgedBytes;
    }

    InterlockedExchange(&txPioPtr->TxBufferLock, 0);
}


#undef _PL011_TX_CPP_