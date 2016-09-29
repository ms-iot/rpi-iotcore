//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011rx.h
//
// Abstract:
//
//    This module contains implementation of methods and callbacks
//    associated with the ARM PL011 UART device driver receive process.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//
#include "precomp.h"
#pragma hdrstop

#define _PL011_RX_CPP_

// Logging header files
#include "PL011logging.h"
#include "PL011rx.tmh"

// Common driver header files
#include "PL011common.h"

// Module specific header files
#include "PL011rx.h"


#ifdef ALLOC_PRAGMA
    #pragma alloc_text(PAGE, PL011RxPioReceiveInit)
#endif // ALLOC_PRAGMA


//
// RX PIO state strings
//
const char* RxPioStateStr[] = {
    "PIO RX", // First entry is the preamble 
    "RX_PIO_STATE__OFF",
    "RX_PIO_STATE__IDLE",
    "RX_PIO_STATE__WAIT_DATA",
    "RX_PIO_STATE__DATA_READY",
    "RX_PIO_STATE__WAIT_READ_DATA",
    "RX_PIO_STATE__READ_DATA",
    "RX_PIO_STATE__PURGE_FIFO",
};
SIZE_T RxPioStateLength = ARRAYSIZE(RxPioStateStr);


//
// Routine Description:
//
//  PL011RxPioReceiveInit is called by PL011pDeviceSerCx2Init to
//  initialize the RX PIO transaction context.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
//  SerCx2PioReceive - The SerCx2 SERCX2PIORECEIVE RX object we created
//      by calling SerCx2PioReceiveCreate.
//
// Return Value:
//
//  Initialization status code
//
_Use_decl_annotations_
NTSTATUS
PL011RxPioReceiveInit(
    WDFDEVICE WdfDevice,
    SERCX2PIORECEIVE SerCx2PioReceive
    )
{
    PAGED_CODE();

    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(SerCx2PioReceive);

    RtlZeroMemory(rxPioPtr, sizeof(PL011_SERCXPIORECEIVE_CONTEXT));

    rxPioPtr->DevExtPtr = devExtPtr;
    rxPioPtr->RxPioState = PL011_RX_PIO_STATE::RX_PIO_STATE__OFF;

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011RxPioReceiveStart is called on first device open to start
//  RX process.
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
PL011RxPioReceiveStart(
    WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(devExtPtr->SerCx2PioReceive);

    (void)PL011RxPioStateSet(
        devExtPtr->SerCx2PioReceive,
        PL011_RX_PIO_STATE::RX_PIO_STATE__OFF
        );

    rxPioPtr->RxBufferIn = 0;
    rxPioPtr->RxBufferOut = 0;
    rxPioPtr->RxBufferCount = 0;
    rxPioPtr->IsLogOverrun = TRUE;

    //
    // Enable RX, and RX timeout interrupts
    //
    PL011HwMaskInterrupts(
        devExtPtr->WdfDevice,
        UARTIMSC_RXIM | UARTIMSC_RTIM,
        FALSE, // unmask
        TRUE // ISR safe
        );

    //
    // Enable RX
    //
    PL011HwUartControl(
        WdfDevice,
        UARTCR_RXE,
        REG_UPDATE_MODE::BITMASK_SET,
        nullptr
        );

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011RxPioReceiveStop is called on last device close to stop
//  RX process.
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
PL011RxPioReceiveStop(
    WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(devExtPtr->SerCx2PioReceive);

    (void)PL011RxPioStateSet(
        devExtPtr->SerCx2PioReceive,
        PL011_RX_PIO_STATE::RX_PIO_STATE__OFF
        );

    RtlZeroMemory(rxPioPtr->RxBuffer, sizeof(rxPioPtr->RxBuffer));

    //
    // Disable RX interrupts
    //
    PL011HwMaskInterrupts(
        WdfDevice,
        UARTIMSC_RXIM | UARTIMSC_RTIM,
        TRUE, // mask
        TRUE  // Non ISR code
        );

    //
    // Disable RX
    //
    PL011HwUartControl(
        WdfDevice,
        UARTCR_RXE,
        REG_UPDATE_MODE::BITMASK_CLEAR,
        nullptr
        );

    return STATUS_SUCCESS;
}


//
// Routine Description:
//
//  PL011SerCx2EvtPioReceiveReadBuffer is called by SerCx2 framework to use 
//  programmed IO (PIO) to transfer data from the receive FIFO to
//  the caller buffer.
//  As long as there is space in caller buffer the routine first copies 
//  copies data collected in RX buffer, and then checks
//  for new RX chars in RX FIFO.
//
// Arguments:
//
//  SerCx2PioReceive - The SerCx2 SERCX2PIORECEIVE RX object we created
//      by calling SerCx2PioReceiveCreate.
//
//  BufferPtr - The caller buffer into which FIFO bytes should be
//      transferred.
//
//  Length - Size in bytes of the caller buffer.
//
// Return Value:
//
//  Number of bytes successfully written to caller buffer.
//
_Use_decl_annotations_
ULONG 
PL011SerCx2EvtPioReceiveReadBuffer(
    SERCX2PIORECEIVE SerCx2PioReceive,
    UCHAR* BufferPtr,
    ULONG Length
    )
{
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr = 
        PL011SerCxPioReceiveGetContext(SerCx2PioReceive);
    PL011_DEVICE_EXTENSION* devExtPtr = rxPioPtr->DevExtPtr;

    (void)PL011RxPioStateSet(
        SerCx2PioReceive,
        PL011_RX_PIO_STATE::RX_PIO_STATE__READ_DATA
        );

    //
    // Copy all RX data to caller's buffer
    //
    ULONG totalBytesCopied = 0;
    while (totalBytesCopied < Length) {
        //
        // RX buffer -> caller buffer
        //
        totalBytesCopied += PL011pRxPioBufferCopy(
            devExtPtr, 
            BufferPtr + totalBytesCopied,
            Length - totalBytesCopied
            );

        //
        // RX FIFO -> RX buffer
        //
        NTSTATUS status = PL011RxPioFifoCopy(devExtPtr, nullptr);
        if (status == STATUS_NO_MORE_FILES) {
            // 
            // RX FIFO is empty...
            //
            break;
        }

    } // Copy RX chars to caller buffer

    if (totalBytesCopied != 0) {

        PL011_LOG_TRACE(
            "PIO RX: read %lu chars, buffer size %lu", 
            totalBytesCopied,
            Length
            );
    }

    PL011_ASSERT(totalBytesCopied <= Length);

    return totalBytesCopied;
}


//
// Routine Description:
//
//  PL011SerCx2EvtPioReceiveEnableReadyNotification is called by SerCx2 
//  to enable the driver to notify SerCx2 when the serial controller 
//  receives new data through SerCx2PioReceiveReady.
//
// Arguments:
//
//  SerCx2PioReceive - The SerCx2 SERCX2PIORECEIVE RX object we created
//      by calling SerCx2PioReceiveCreate.
//
// Return Value:
//
_Use_decl_annotations_
VOID 
PL011SerCx2EvtPioReceiveEnableReadyNotification(
    SERCX2PIORECEIVE SerCx2PioReceive
    )
{
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(SerCx2PioReceive);

    //
    // Reset the RX state so we can tell if new RX data 
    // arrived (through ISR).
    //
    (void)PL011RxPioStateSet(
        SerCx2PioReceive,
        PL011_RX_PIO_STATE::RX_PIO_STATE__READ_DATA
        );

    //
    // We may have new data by now...
    //
    if (PL011RxPendingByteCount(rxPioPtr) > 0) {

        SerCx2PioReceiveReady(SerCx2PioReceive);
        return;
    }

    //
    // Mark that we are waiting for new data through RX interrupt.
    // The driver will not call SerCx2PioReceiveReady unless 
    // PIO RX state is RX_PIO_STATE__WAIT_DATA.
    //
    // If we already have new RX data, means is the 
    // current state is RX_PIO_STATE__DATA_READY rather than
    // RX_PIO_STATE__READ_DATA, tell SerCx2 to come and get it...
    //
    if (!PL011RxPioStateSetCompare(
            SerCx2PioReceive,
            PL011_RX_PIO_STATE::RX_PIO_STATE__WAIT_DATA,
            PL011_RX_PIO_STATE::RX_PIO_STATE__READ_DATA
            )) {
        //
        // More RX data can already be ready...
        //
        PL011_ASSERT(
            PL011RxPioStateGet(SerCx2PioReceive) ==
            PL011_RX_PIO_STATE::RX_PIO_STATE__DATA_READY
            );
        PL011_ASSERT(PL011RxPendingByteCount(rxPioPtr) != 0);

        SerCx2PioReceiveReady(SerCx2PioReceive);
        return;
    }
}


//
// Routine Description:
//
//  PL011SerCx2EvtPioReceiveCancelReadyNotification is called by SerCx2 
//  to cancel a previous call to
//  PL011SerCx2EvtPioReceiveEnableReadyNotification.
//
// Arguments:
//
//  SerCx2PioReceive - The SerCx2 SERCX2PIORECEIVE RX object we created
//      by calling SerCx2PioReceiveCreate.
//
// Return Value:
//
//  TRUE - 'Ready Notifications' were successfully disabled, or FALSE if
//      SerCx2PioReceiveReady was called or is about to be called. 
//
_Use_decl_annotations_
BOOLEAN 
PL011SerCx2EvtPioReceiveCancelReadyNotification(
    SERCX2PIORECEIVE SerCx2PioReceive
    )
{
    //
    // Change RX state to idle, so if new data canceled 
    // was received, and DPC was already scheduled, it
    // will not call SerCx2PioReceiveReady().
    //
    PL011_RX_PIO_STATE prevRxPioState = PL011RxPioStateSet(
        SerCx2PioReceive,
        PL011_RX_PIO_STATE::RX_PIO_STATE__IDLE
        );
    BOOLEAN isCanceled =
        prevRxPioState != PL011_RX_PIO_STATE::RX_PIO_STATE__WAIT_READ_DATA;

    PL011_LOG_TRACE(
        "PIO RX Cancel Notifications: -> %d",
        isCanceled
        );

    return isCanceled;
}


//
// Routine Description:
//
//  PL011RxPioPurgeFifo is called by PL011EvtSerCx2PurgeFifos to purge 
//  the RX FIFO either PIO or DMA.
//
//  The routine discards all pending RX chars.
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
PL011RxPurgeFifo(
    WDFDEVICE WdfDevice,
    ULONG* PurgedBytesPtr
    )
{
    //
    // Purge PIO RX buffer/FIFO
    //
    PL011pRxPioPurgeFifo(WdfDevice, PurgedBytesPtr);
}


//
// Routine Description:
//
//  PL011RxPioFifoCopy is called to copy new RX data from RX FIFO 
//  to RX buffer.
//  The routine can be called from PL011SerCx2EvtPioReceiveReadBuffer
//  or RX interrupt.
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
//  STATUS_SUCCESS - Data was successfully copied into RX buffer.
//  STATUS_NO_MORE_FILES - RX FIFO is empty, no chars were copied.
//  STATUS_DEVICE_BUSY - A previous call to  PL011RxPioFifoCopy is currently in
//      progress.
//  STATUS_BUFFER_OVERFLOW - RX buffer is full, RX was not fully read.
//
_Use_decl_annotations_
NTSTATUS
PL011RxPioFifoCopy(
    PL011_DEVICE_EXTENSION* DevExtPtr,
    ULONG* CharsCopiedPtr
    )
{
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(DevExtPtr->SerCx2PioReceive);

    if (CharsCopiedPtr != nullptr) {

        *CharsCopiedPtr = 0;
    }

    //
    // Verify access to RX buffer
    //
    if (InterlockedExchange(&rxPioPtr->RxBufferLock, 1) != 0) {

        return STATUS_DEVICE_BUSY;
    }

    //
    // Used register addresses
    //
    // Data register address
    volatile ULONG* regUARTDRPtr = PL011HwRegAddress(DevExtPtr, UARTDR);
    // Flags register address
    volatile ULONG* regUARTFRPtr = PL011HwRegAddress(DevExtPtr, UARTFR);

    NTSTATUS status = STATUS_SUCCESS;
    ULONG charsTransferred = 0;
    ULONG rxIn = rxPioPtr->RxBufferIn;
    volatile LONG* rxPendingCountPtr = &rxPioPtr->RxBufferCount;

    //
    // Read received words from RX FIFO to RX buffer
    //
    while (PL011RxPendingByteCount(rxPioPtr) < PL011_RX_BUFFER_SIZE_BYTES) {
        //
        // Check if RX FIFO is empty
        //
        if ((PL011HwReadRegisterUlong(regUARTFRPtr) & UARTFR_RXFE) != 0) {

            rxPioPtr->IsLogOverrun = TRUE;

            if ((charsTransferred == 0) &&
                (PL011RxPendingByteCount(rxPioPtr) == 0)) {

                status = STATUS_NO_MORE_FILES;

            } else {

                status = STATUS_SUCCESS;
            }
            break;

        }  // RX FIFO is empty

        //
        // Read next word from RX FIFO
        //
        rxPioPtr->RxBuffer[rxIn] = 
            UCHAR(PL011HwReadRegisterUlongNoFence(regUARTDRPtr));

        ++charsTransferred;
        InterlockedIncrement(rxPendingCountPtr);

        rxIn = (rxIn + 1) % PL011_RX_BUFFER_SIZE_BYTES;

    } // While RX buffer not full

    rxPioPtr->RxBufferIn = rxIn;

    //
    // Check for buffer overflow
    //
    if (PL011RxPendingByteCount(rxPioPtr) >= PL011_RX_BUFFER_SIZE_BYTES) {

        status = STATUS_BUFFER_OVERFLOW;
        if (rxPioPtr->IsLogOverrun) {

            PL011_LOG_WARNING(
                "RX buffer full!, (status = %!STATUS!)", status
                );
            rxPioPtr->IsLogOverrun = FALSE;
        }
    }

    if (charsTransferred != 0) {

        PL011_LOG_TRACE(
            "RX FIFO: read %lu chars, in %lu, out %lu, count %lu",
            charsTransferred,
            rxPioPtr->RxBufferIn,
            rxPioPtr->RxBufferOut,
            rxPioPtr->RxBufferCount
            );
    }

    (void)InterlockedExchange(&rxPioPtr->RxBufferLock, 0);

    if (CharsCopiedPtr != nullptr) {

        *CharsCopiedPtr = charsTransferred;
    }

    return status;
}


//
// Routine Description:
//
//  PL011pRxPioBufferCopy is called to copy new RX data from PIO RX buffer 
//  to the caller RX buffer.
//
// Arguments:
//
//  DevExtPtr - Our device context.
//
//  BufferPtr - The caller buffer into which FIFO bytes should be
//      transferred.
//
//  Length - Size in bytes of the caller buffer.
//
// Return Value:
//
//  Number of bytes successfully written to caller buffer.
//
_Use_decl_annotations_
ULONG
PL011pRxPioBufferCopy(
    PL011_DEVICE_EXTENSION* DevExtPtr,
    UCHAR* BufferPtr,
    ULONG Length
    )
{
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(DevExtPtr->SerCx2PioReceive);

    //
    // Get number of bytes we can copy.
    // Is RX buffer empty ?
    //
    ULONG bytesToCopy = PL011RxPendingByteCount(rxPioPtr);
    bytesToCopy = min(bytesToCopy, Length);
    if (bytesToCopy == 0) {

        return 0;
    }

    //
    // Copy RX data: RX Buffer -> Caller buffer
    //
    ULONG rxOut = rxPioPtr->RxBufferOut;
    ULONG bytesCopied = min(bytesToCopy, (PL011_RX_BUFFER_SIZE_BYTES - rxOut));

    _Analysis_assume_(bytesCopied <= Length);
    RtlCopyMemory(BufferPtr, &rxPioPtr->RxBuffer[rxOut], bytesCopied);

    rxOut = (rxOut + bytesCopied) % PL011_RX_BUFFER_SIZE_BYTES;

    if (bytesCopied < bytesToCopy) {

        BufferPtr += bytesCopied;

        ULONG bytesLeftToCopy = bytesToCopy - bytesCopied;
        RtlCopyMemory(BufferPtr, &rxPioPtr->RxBuffer, bytesLeftToCopy);

        bytesCopied += bytesLeftToCopy;
        rxOut = bytesLeftToCopy;

    } // if (bytesCopied < bytesToCopy)

    rxPioPtr->RxBufferOut = rxOut;
    InterlockedAdd(&rxPioPtr->RxBufferCount, -LONG(bytesCopied));

    if (bytesCopied != 0) {

        PL011_LOG_TRACE(
            "RX buffer: read %lu chars, buffer length %lu, in %lu, out %lu, count %lu",
            bytesCopied,
            Length,
            rxPioPtr->RxBufferIn,
            rxPioPtr->RxBufferOut,
            rxPioPtr->RxBufferCount
            );
    }

    return bytesCopied;
}


//
// Routine Description:
//
//  PL011pRxPioPurgeFifo is called by PL011RxPioPurgeFifo to purge 
//  the PIO RX FIFO.
//
//  The routine discards all pending RX chars.
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
PL011pRxPioPurgeFifo(
    WDFDEVICE WdfDevice,
    ULONG* PurgedBytesPtr
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(devExtPtr->SerCx2PioReceive);

    PL011_LOG_INFORMATION(
        "RX purge FIFO!"
        );

    (void)PL011RxPioStateSet(
        devExtPtr->SerCx2PioReceive,
        PL011_RX_PIO_STATE::RX_PIO_STATE__PURGE_FIFO
        );

    if (InterlockedExchange(&rxPioPtr->RxBufferLock, 1) != 0) {
        //
        // We should not get here
        //
        PL011_ASSERT(FALSE);
    }

    //
    // Read all data from RX FIFO...
    //

    // Data register address
    volatile ULONG* regUARTDRPtr = PL011HwRegAddress(devExtPtr, UARTDR);
    ULONG purgedBytes = 0;

    while (!PL011HwIsRxFifoEmpty(devExtPtr)) {

        PL011HwReadRegisterUlong(regUARTDRPtr);
        ++purgedBytes;

    } // while (RX FIFO not empty)

    purgedBytes += rxPioPtr->RxBufferCount;

    rxPioPtr->RxBufferCount = 0;
    rxPioPtr->RxBufferIn = 0;
    rxPioPtr->RxBufferOut = 0;

    //
    // Complete the TX FIFO purge...
    //

    (void)PL011RxPioStateSet(
        devExtPtr->SerCx2PioReceive,
        PL011_RX_PIO_STATE::RX_PIO_STATE__IDLE
        );

    if (PurgedBytesPtr != nullptr) {

        *PurgedBytesPtr = purgedBytes;
    }

    (void)InterlockedExchange(&rxPioPtr->RxBufferLock, 0);

    PL011_LOG_INFORMATION(
        "RX purge FIFO Done!"
        );
}


#undef _PL011_RX_CPP_