//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name: 
//
//    PL011rx.h
//
// Abstract:
//
//    This module contains enum, types, methods, and callback definitions
//    associated with the ARM PL011 UART device driver receive process.
//    This controller driver uses the Serial WDF class extension (SerCx2).
//
// Environment:
//
//    kernel-mode only
//

#ifndef _PL011_RX_H_
#define _PL011_RX_H_

WDF_EXTERN_C_START


//
// RX/TX circular buffers size in bytes
//
enum : ULONG { PL011_RX_BUFFER_SIZE_BYTES = 8 * 1024};

//
// Globals
//
extern const char* RxPioStateStr[];
extern SIZE_T RxPioStateLength;


//
// Forward declaration of the device context
//
struct _PL011_DEVICE_EXTENSION;


//
// PL011 RX PIO state codes
//
typedef enum _PL011_RX_PIO_STATE : LONG {

    RX_PIO_STATE__OFF = 0,
    RX_PIO_STATE__IDLE,
    RX_PIO_STATE__WAIT_DATA,
    RX_PIO_STATE__DATA_READY,
    RX_PIO_STATE__WAIT_READ_DATA,
    RX_PIO_STATE__READ_DATA,
    RX_PIO_STATE__PURGE_FIFO,

    // Always last
    RX_PIO_STATE__MAX

} PL011_RX_PIO_STATE;


//
// PL011_SERCXPIORECEIVE_CONTEXT
//
typedef struct _PL011_SERCXPIORECEIVE_CONTEXT
{
    //
    // The device extension
    //
    struct _PL011_DEVICE_EXTENSION* DevExtPtr;

    //
    // RX PIO state
    //
    PL011_RX_PIO_STATE RxPioState;

    //
    // RX circular buffer
    //
    volatile LONG   RxBufferLock;
    ULONG           RxBufferIn;
    ULONG           RxBufferOut;
    volatile LONG   RxBufferCount;
    UCHAR           RxBuffer[PL011_RX_BUFFER_SIZE_BYTES];

    //
    // If to log overrun
    //
    BOOLEAN         IsLogOverrun;

} PL011_SERCXPIORECEIVE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PL011_SERCXPIORECEIVE_CONTEXT, PL011SerCxPioReceiveGetContext);


//
// PL011_SERCXSYSTEMDMARECEIVE_CONTEXT
//
typedef struct _PL011_SERCXSYSTEMDMARECEIVE_CONTEXT
{
    //
    // The device extension
    //
    struct _PL011_DEVICE_EXTENSION* DevExtPtr;

} PL011_SERCXSYSTEMDMARECEIVE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PL011_SERCXSYSTEMDMARECEIVE_CONTEXT, PL011DeviceGetSerCxSystemDmaReceiveContext);


//
// The PL011 SerCx2 device receive event handlers
//
EVT_SERCX2_PIO_RECEIVE_ENABLE_READY_NOTIFICATION PL011SerCx2EvtPioReceiveEnableReadyNotification;
EVT_SERCX2_PIO_RECEIVE_CANCEL_READY_NOTIFICATION PL011SerCx2EvtPioReceiveCancelReadyNotification;
EVT_SERCX2_PIO_RECEIVE_READ_BUFFER PL011SerCx2EvtPioReceiveReadBuffer;

EVT_SERCX2_SYSTEM_DMA_RECEIVE_ENABLE_NEW_DATA_NOTIFICATION PL011SerCx2EvtDmaReceiveEnableNewDataNotification;
EVT_SERCX2_SYSTEM_DMA_RECEIVE_CANCEL_NEW_DATA_NOTIFICATION PL011SerCx2EvtDmaReceiveCancelNewDataNotification;


//
// PL011rx public methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
PL011RxPioReceiveInit(
    _In_ WDFDEVICE WdfDevice,
    _In_ SERCX2PIORECEIVE SerCx2PioReceive
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011RxPioReceiveStart(
    _In_ WDFDEVICE WdfDevice
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
PL011RxPioReceiveStop(
    _In_ WDFDEVICE WdfDevice
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
PL011RxPurgeFifo(
    _In_ WDFDEVICE WdfDevice,
    _Out_opt_ ULONG* PurgedBytesPtr
    );

NTSTATUS
PL011RxPioFifoCopy(
    _In_ PL011_DEVICE_EXTENSION* DevExtPtr,
    _Out_opt_ ULONG* CharsCopiedPtr
    );

//
// Routine Description:
//
//  PL011RxPioStateSet is called to set the next RX PIO state.
//
// Arguments:
//
//  SerCx2PioReceive - The SerCx2 SERCX2PIORECEIVE RX object we created
//      by calling SerCx2PioReceiveCreate.
//
//  NextRxPioState - The next RX PIO state.
//
// Return Value:
//  
//  The previous RX PIO state
//
__forceinline
PL011_RX_PIO_STATE
PL011RxPioStateSet(
    _In_ SERCX2PIORECEIVE SerCx2PioReceive,
    _In_ PL011_RX_PIO_STATE NextRxPioState
    )
{
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(SerCx2PioReceive);

    NT_ASSERT(
        ULONG(NextRxPioState) < ULONG(PL011_RX_PIO_STATE::RX_PIO_STATE__MAX)
        );

#if DBG
    return PL011_RX_PIO_STATE(PL011StateSet(
        reinterpret_cast<ULONG*>(&rxPioPtr->RxPioState),
        ULONG(NextRxPioState),
        RxPioStateStr,
        RxPioStateLength
        ));
#else // DBG
    return PL011_RX_PIO_STATE(PL011StateSet(
        reinterpret_cast<ULONG*>(&rxPioPtr->RxPioState),
        ULONG(NextRxPioState)
        ));
#endif // !DBG
}

//
// Routine Description:
//
//  PL011RxPioStateSetCompare is called to set the next RX PIO state, if
//  the current state is equal to the comparand state.
//
// Arguments:
//
//  SerCx2PioReceive - The SerCx2 SERCX2PIORECEIVE RX object we created
//      by calling SerCx2PioReceiveCreate.
//
//  NextRxPioState - The next RX PIO state.
//
//  ComparetRxPioState - The RX PIO state to compare with.
//
// Return Value:
//  
//  TRUE if NextRxPioState was set, otherwise FALSE.
//
__forceinline
BOOLEAN
PL011RxPioStateSetCompare(
    _In_ SERCX2PIORECEIVE SerCx2PioReceive,
    _In_ PL011_RX_PIO_STATE NextRxPioState,
    _In_ PL011_RX_PIO_STATE ComparetRxPioState
    )
{
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(SerCx2PioReceive);

    NT_ASSERT(
       ULONG(NextRxPioState) < ULONG(PL011_RX_PIO_STATE::RX_PIO_STATE__MAX)
       );
    NT_ASSERT(
       ULONG(ComparetRxPioState) < ULONG(PL011_RX_PIO_STATE::RX_PIO_STATE__MAX)
       );

#if DBG
    return PL011StateSetCompare(
        reinterpret_cast<ULONG*>(&rxPioPtr->RxPioState),
        ULONG(NextRxPioState),
        ULONG(ComparetRxPioState),
        RxPioStateStr,
        RxPioStateLength
        );
#else // DBG
    return PL011StateSetCompare(
        reinterpret_cast<ULONG*>(&rxPioPtr->RxPioState),
        ULONG(NextRxPioState),
        ULONG(ComparetRxPioState)
        );
#endif // !DBG
}

//
// Routine Description:
//
//  PL011RxPioStateGet is called to get the current RX PIO state.
//
// Arguments:
//
//  SerCx2PioReceive - The SerCx2 SERCX2PIORECEIVE RX object we created
//      by calling SerCx2PioReceiveCreate.
//
// Return Value:
//
//  The current RX PIO state.
//
__forceinline
PL011_RX_PIO_STATE
PL011RxPioStateGet(
    _In_ SERCX2PIORECEIVE SerCx2PioReceive
    )
{
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(SerCx2PioReceive);

    return PL011_RX_PIO_STATE(PL011StateGet(
        reinterpret_cast<ULONG*>(&rxPioPtr->RxPioState)
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
PL011RxGetInQueue(
    _In_ WDFDEVICE WdfDevice
    )
{
    PL011_DEVICE_EXTENSION* devExtPtr = PL011DeviceGetExtension(WdfDevice);
    PL011_SERCXPIORECEIVE_CONTEXT* rxPioPtr =
        PL011SerCxPioReceiveGetContext(devExtPtr->SerCx2PioReceive);

    return ULONG(InterlockedAdd(
        reinterpret_cast<volatile LONG*>(&rxPioPtr->RxBufferCount),
        0
        ));
}

//
// Routine Description:
//
//  PL011RxPendingByteCount is called to get the current number of received 
//  bytes that are waiting in the RX buffer.
//
// Arguments:
//
//  RxPioPtr - Our PL011_SERCXPIORECEIVE_CONTEXT.
//
// Return Value:
//
//  The current number received bytes that are waiting in the RX buffer.
//
__forceinline
ULONG
PL011RxPendingByteCount(
    _In_ PL011_SERCXPIORECEIVE_CONTEXT* RxPioPtr
    )
{
    LONG rxPendingByteCount = InterlockedAdd(&RxPioPtr->RxBufferCount, 0);
    NT_ASSERT(rxPendingByteCount >= 0);

    return ULONG(rxPendingByteCount);
}


//
// PL011rx private methods
//
#ifdef _PL011_RX_CPP_

    _IRQL_requires_max_(DISPATCH_LEVEL)
    VOID
    PL011pRxPioPurgeFifo(
        _In_ WDFDEVICE WdfDevice,
        _Out_opt_ ULONG* PurgedBytesPtr
        );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG
    PL011pRxPioBufferCopy(
        _In_ PL011_DEVICE_EXTENSION* DevExtPtr,
        _Out_writes_(Length) UCHAR* BufferPtr,
        _In_ ULONG Length
        );

#endif //_PL011_RX_CPP_


WDF_EXTERN_C_END

#endif // !_PL011_RX_H_
