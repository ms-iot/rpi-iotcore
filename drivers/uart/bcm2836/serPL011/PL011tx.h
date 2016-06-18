//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011tx.h
//
// Abstract:
//
//    This module contains enum, types, methods, and callback definitions
//    associated with the ARM PL011 UART device driver transmit process.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//

#ifndef _PL011_TX_H_
#define _PL011_TX_H_

WDF_EXTERN_C_START

//
// TX circular buffers size in bytes
//
enum : ULONG { PL011_TX_BUFFER_SIZE_BYTES = 1 * PL011_FIFO_DEPTH };


//
// Globals
//
extern const char* TxPioStateStr[];
extern SIZE_T TxPioStateLength;


//
// Forward declaration of the device context
//
struct _PL011_DEVICE_EXTENSION;


//
// PL011 TX PIO state codes
//
typedef enum _PL011_TX_PIO_STATE : LONG {

    TX_PIO_STATE__OFF = 0,
    TX_PIO_STATE__IDLE,
    TX_PIO_STATE__SEND_DATA,
    TX_PIO_STATE__WAIT_DATA_SENT,
    TX_PIO_STATE__DATA_SENT,
    TX_PIO_STATE__WAIT_SEND_DATA, 
    TX_PIO_STATE__DRAIN_FIFO,
    TX_PIO_STATE__PURGE_FIFO,

    // Always last
    TX_PIO_STATE__MAX

} PL011_TX_PIO_STATE;


//
// PL011_SERCXPIOTRANSMIT_CONTEXT
//
typedef struct _PL011_SERCXPIOTRANSMIT_CONTEXT
{
    //
    // The device extension
    //
    struct _PL011_DEVICE_EXTENSION* DevExtPtr;

    //
    // TX PIO state
    //
    PL011_TX_PIO_STATE TxPioState;

    //
    // TX circular buffer
    //
    volatile LONG   TxBufferLock;
    ULONG           TxBufferIn;
    ULONG           TxBufferOut;
    volatile LONG   TxBufferCount;
    UCHAR           TxBuffer[PL011_TX_BUFFER_SIZE_BYTES];

} PL011_SERCXPIOTRANSMIT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PL011_SERCXPIOTRANSMIT_CONTEXT, PL011SerCxPioTransmitGetContext);


//
// PL011_SERCXSYSTEMDMATRANSMIT_CONTEXT
//
typedef struct _PL011_SERCXSYSTEMDMATRANSMIT_CONTEXT
{
    struct _PL011_DEVICE_EXTENSION* DevExtPtr;

} PL011_SERCXSYSTEMDMATRANSMIT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PL011_SERCXSYSTEMDMATRANSMIT_CONTEXT, PL011DeviceGetSerCxSystemDmaTransmitContext);


//
// The PL011 SerCx2 device transmit event handlers
//
EVT_SERCX2_PIO_TRANSMIT_ENABLE_READY_NOTIFICATION PL011SerCx2EvtPioTransmitEnableReadyNotification;
EVT_SERCX2_PIO_TRANSMIT_CANCEL_READY_NOTIFICATION PL011SerCx2EvtPioTransmitCancelReadyNotification;
EVT_SERCX2_PIO_TRANSMIT_WRITE_BUFFER PL011SerCx2EvtPioTransmitWriteBuffer;

EVT_SERCX2_PIO_TRANSMIT_DRAIN_FIFO PL011SerCx2EvtPioTransmitDrainFifo;
EVT_SERCX2_PIO_TRANSMIT_CANCEL_DRAIN_FIFO PL011SerCx2EvtPioTransmitCancelDrainFifo;
EVT_SERCX2_PIO_TRANSMIT_PURGE_FIFO PL011SerCx2EvtPioTransmitPurgeFifo;

EVT_SERCX2_SYSTEM_DMA_TRANSMIT_DRAIN_FIFO PL011SerCx2EvtSystemDmaTransmitDrainFifo;
EVT_SERCX2_SYSTEM_DMA_TRANSMIT_CANCEL_DRAIN_FIFO PL011SerCx2EvtSystemDmaTransmitCancelDrainFifo;
EVT_SERCX2_SYSTEM_DMA_TRANSMIT_PURGE_FIFO PL011SerCx2EvtSystemDmaTransmitPurgeFifo;


//
// PL011tx public methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
PL011TxPioTransmitInit(
    _In_ WDFDEVICE WdfDevice,
    _In_ SERCX2PIOTRANSMIT SerCx2PioTransmit
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011TxPioTransmitStart(
    _In_ WDFDEVICE WdfDevice
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011TxPioTransmitStop(
    _In_ WDFDEVICE WdfDevice
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
PL011TxPurgeFifo(
    _In_ WDFDEVICE WdfDevice,
    _Out_opt_ ULONG* PurgedBytesPtr
    );

NTSTATUS
PL011TxPioFifoCopy(
    _In_ PL011_DEVICE_EXTENSION* DevExtPtr,
    _Out_opt_ ULONG* CharsCopiedPtr
    );

//
// Routine Description:
//
//  PL011TxPioStateSet is called to set the next TX PIO state.
//
// Arguments:
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
//  NextTxPioState - The next TX PIO state.
//
// Return Value:
//  
//  The previous TX PIO state
//
__forceinline
PL011_TX_PIO_STATE
PL011TxPioStateSet(
    _In_ SERCX2PIOTRANSMIT SerCx2PioTransmit,
    _In_ PL011_TX_PIO_STATE NextTxPioState
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(SerCx2PioTransmit);

    NT_ASSERT(
        ULONG(NextTxPioState) < ULONG(PL011_TX_PIO_STATE::TX_PIO_STATE__MAX)
        );

#if DBG
    return PL011_TX_PIO_STATE(PL011StateSet(
        reinterpret_cast<ULONG*>(&txPioPtr->TxPioState),
        ULONG(NextTxPioState),
        TxPioStateStr,
        TxPioStateLength
        ));
#else // DBG
    return PL011_TX_PIO_STATE(PL011StateSet(
        reinterpret_cast<ULONG*>(&txPioPtr->TxPioState),
        ULONG(NextTxPioState)
        ));
#endif // !DBG
}

//
// Routine Description:
//
//  PL011TxPioStateSetCompare is called to set the next TX PIO state, if
//  the current state is equal to the comparand state.
//
// Arguments:
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
//  NextTxPioState - The next TX PIO state.
//
//  ComparetTxPioState - The TX PIO state to compare with.
//
// Return Value:
//  
//  TRUE if NextTxPioState was set, otherwise FALSE.
//
__forceinline
BOOLEAN
PL011TxPioStateSetCompare(
    SERCX2PIOTRANSMIT SerCx2PioTransmit,
    PL011_TX_PIO_STATE NextTxPioState,
    PL011_TX_PIO_STATE ComparetTxPioState
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(SerCx2PioTransmit);

    NT_ASSERT(
        ULONG(NextTxPioState) < ULONG(PL011_TX_PIO_STATE::TX_PIO_STATE__MAX)
        );
    NT_ASSERT(
        ULONG(ComparetTxPioState) < ULONG(PL011_TX_PIO_STATE::TX_PIO_STATE__MAX)
        );

#if DBG
    return PL011StateSetCompare(
        reinterpret_cast<ULONG*>(&txPioPtr->TxPioState),
        ULONG(NextTxPioState),
        ComparetTxPioState,
        TxPioStateStr,
        TxPioStateLength
        );
#else // DBG
    return PL011StateSetCompare(
        reinterpret_cast<ULONG*>(&txPioPtr->TxPioState),
        ULONG(NextTxPioState),
        ComparetTxPioState
        );
#endif // !DBG
}

//
// Routine Description:
//
//  PL011TxPioStateGet is called to get the current TX PIO state.
//
// Arguments:
//
//  SerCx2PioTransmit - The SerCx2 SERCX2PIOTRANSMIT TX object we created
//      by calling SerCx2PioTransmitCreate.
//
// Return Value:
//
//  The current TX PIO state.
//
__forceinline
PL011_TX_PIO_STATE
PL011TxPioStateGet(
    SERCX2PIOTRANSMIT SerCx2PioTransmit
    )
{
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(SerCx2PioTransmit);

    return PL011_TX_PIO_STATE(PL011StateGet(
        reinterpret_cast<ULONG*>(&txPioPtr->TxPioState)
        ));
}

//
// Routine Description:
//
//  PL011RxGetInQueue is called to get the current number received 
//  bytes that are waiting in the RX buffer.
//
// Arguments:
//
//  WdfDevice - The WdfDevice object the represent the PL011 this instance of
//      the PL011 controller.
//
// Return Value:
//
//  The current number received bytes that are waiting in the RX buffer.
//
__forceinline
ULONG
PL011TxGetOutQueue(
    _In_ WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIOTRANSMIT_CONTEXT* txPioPtr =
        PL011SerCxPioTransmitGetContext(devExtPtr->SerCx2PioTransmit);

    return ULONG(InterlockedAdd(
        reinterpret_cast<volatile LONG*>(&txPioPtr->TxBufferCount),
        0
        ));
}

//
// Routine Description:
//
//  PL011TxPendingByteCount is called to get the current number of  
//  bytes that are waiting in the TX buffer.
//
// Arguments:
//
//  TxPioPtr - Our PL011_SERCXPIOTRANSMIT_CONTEXT.
//
// Return Value:
//
//  The current number received bytes that are waiting in the RX buffer.
//
__forceinline
ULONG
PL011TxPendingByteCount(
    _In_ PL011_SERCXPIOTRANSMIT_CONTEXT* TxPioPtr
    )
{
    LONG txPendingByteCount = InterlockedAdd(&TxPioPtr->TxBufferCount, 0);
    NT_ASSERT(txPendingByteCount <= PL011_TX_BUFFER_SIZE_BYTES);

    return ULONG(txPendingByteCount);
}


//
// PL011tx private methods
//
#ifdef _PL011_TX_CPP_

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG
    PL011pTxPioBufferCopy(
        _In_ PL011_DEVICE_EXTENSION* DevExtPtr,
        _In_reads_(Length) const UCHAR* BufferPtr,
        _In_ ULONG Length
        );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    VOID
    PL011pTxPioPurgeFifo(
        _In_ WDFDEVICE WdfDevice,
        _Out_opt_ ULONG* PurgedBytesPtr
        );

#endif //_PL011_TX_CPP_


WDF_EXTERN_C_END

#endif // !_PL011_TX_H_
