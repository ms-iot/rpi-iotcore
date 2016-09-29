// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    ioctl.c
//
// Abstract:
//
//    This module contains the ioctl dispatcher as well as a couple
//    of routines that are generally just called in response to
//    ioctl calls.

#include "precomp.h"
#include "ioctl.tmh"

EVT_WDF_INTERRUPT_SYNCHRONIZE SerialGetModemUpdate;
EVT_WDF_INTERRUPT_SYNCHRONIZE SerialGetCommStatus;
EVT_WDF_INTERRUPT_SYNCHRONIZE SerialSetEscapeChar;

/*++

Routine Description:

    In sync with the interrpt service routine (which sets the perf stats)
    return the perf stats to the caller.


Arguments:

    Context - Pointer to a the request.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialGetStats(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PREQUEST_CONTEXT reqContext = (PREQUEST_CONTEXT)Context;
    PSERIAL_DEVICE_EXTENSION extension = SerialGetDeviceExtension(WdfInterruptGetDevice(Interrupt));
    PSERIALPERF_STATS sp = reqContext->SystemBuffer;

    UNREFERENCED_PARAMETER(Interrupt);

    *sp = extension->PerfStats;
    return FALSE;
}

/*++

Routine Description:

    In sync with the interrpt service routine (which sets the perf stats)
    clear the perf stats.


Arguments:

    Context - Pointer to a the extension.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialClearStats(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    UNREFERENCED_PARAMETER(Interrupt);

    RtlZeroMemory(&((PSERIAL_DEVICE_EXTENSION)Context)->PerfStats,
        sizeof(SERIALPERF_STATS));

    RtlZeroMemory(&((PSERIAL_DEVICE_EXTENSION)Context)->WmiPerfData,
                 sizeof(SERIAL_WMI_PERF_DATA));

    return FALSE;
}


/*++

Routine Description:

    This routine is used to set the special characters for the
    driver.

Arguments:

    Context - Pointer to a structure that contains a pointer to
              the device extension and a pointer to a special characters
              structure.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialSetChars(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    UNREFERENCED_PARAMETER(Interrupt);

    ((PSERIAL_IOCTL_SYNC)Context)->Extension->SpecialChars =
        *((PSERIAL_CHARS)(((PSERIAL_IOCTL_SYNC)Context)->Data));

    return FALSE;
}

/*++

Routine Description:

    This routine is used to set the baud rate of the device.

Arguments:

    Context - Pointer to a structure that contains a pointer to
              the device extension and what should be the current
              baud rate.

Return Value:

    This routine always returns FALSE.

--*/

_Use_decl_annotations_
BOOLEAN
SerialSetBaud(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = ((PSERIAL_IOCTL_SYNC)Context)->Extension;
    USHORT appropriate = PtrToUshort(((PSERIAL_IOCTL_SYNC)Context)->Data);

    UNREFERENCED_PARAMETER(Interrupt);

    WRITE_DIVISOR_LATCH(extension,
                        extension->Controller,
                        appropriate);

    return FALSE;
}

/*++

Routine Description:

    This routine is used to set the buad rate of the device.

Arguments:

    Context - Pointer to the device extension.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialSetLineControl(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = (PSERIAL_DEVICE_EXTENSION)Context;

    UNREFERENCED_PARAMETER(Interrupt);

    WRITE_LINE_CONTROL(extension,
        extension->Controller,
        extension->LineControl);

    return FALSE;
}

/*++

Routine Description:

    This routine is simply used to call the interrupt level routine
    that handles modem status update.

Arguments:

    Context - Pointer to a structure that contains a pointer to
              the device extension and a pointer to a ulong.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialGetModemUpdate(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = ((PSERIAL_IOCTL_SYNC)Context)->Extension;
    ULONG* result = (ULONG*)(((PSERIAL_IOCTL_SYNC)Context)->Data);

    UNREFERENCED_PARAMETER(Interrupt);

    *result = SerialHandleModemUpdate(extension,
                                    FALSE);

    return FALSE;
}


/*++

Routine Description:

    This routine is simply used to set the contents of the MCR

Arguments:

    Context - Pointer to a structure that contains a pointer to
              the device extension and a pointer to a ulong.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialSetMCRContents(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
   PSERIAL_DEVICE_EXTENSION extension = ((PSERIAL_IOCTL_SYNC)Context)->Extension;
   ULONG* result = (ULONG*)(((PSERIAL_IOCTL_SYNC)Context)->Data);

   UNREFERENCED_PARAMETER(Interrupt);

   WRITE_MODEM_CONTROL(extension,
                        extension->Controller,
                        (UCHAR)PtrToUlong(result));

   return FALSE;
}


/*++

Routine Description:

    This routine is simply used to get the contents of the MCR

Arguments:

    Context - Pointer to a structure that contains a pointer to
              the device extension and a pointer to a ulong.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialGetMCRContents(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = ((PSERIAL_IOCTL_SYNC)Context)->Extension;
    ULONG* result = (ULONG*)(((PSERIAL_IOCTL_SYNC)Context)->Data);

    UNREFERENCED_PARAMETER(Interrupt);

    *result = READ_MODEM_CONTROL(extension, extension->Controller);

    return FALSE;
}

/*++

Routine Description:

    This routine is simply used to set the contents of the FCR

Arguments:

    Context - Pointer to a structure that contains a pointer to
              the device extension and a pointer to a ulong.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialSetFCRContents(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = ((PSERIAL_IOCTL_SYNC)Context)->Extension;
    ULONG* result = (ULONG*)(((PSERIAL_IOCTL_SYNC)Context)->Data);

    UNREFERENCED_PARAMETER(Interrupt);

    WRITE_FIFO_CONTROL(extension,
                        extension->Controller,
                        (UCHAR)*result);

    return FALSE;
}


/*++

Routine Description:

    This is used to get the current state of the serial driver.

Arguments:

    Context - Pointer to a structure that contains a pointer to
              the device extension and a pointer to a serial status
              record.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialGetCommStatus(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PSERIAL_DEVICE_EXTENSION extension = ((PSERIAL_IOCTL_SYNC)Context)->Extension;
    PSERIAL_STATUS stat = ((PSERIAL_IOCTL_SYNC)Context)->Data;

    UNREFERENCED_PARAMETER(Interrupt);

    stat->Errors = extension->ErrorWord;
    extension->ErrorWord = 0;

    // Eof isn't supported in binary mode

    stat->EofReceived = FALSE;

    stat->AmountInInQueue = extension->CharsInInterruptBuffer;

    stat->AmountInOutQueue = extension->TotalCharsQueued;

    if (extension->WriteLength) {

    // By definition if we have a writelength the we have
    // a current write request.

     PREQUEST_CONTEXT reqContext = NULL;

        ASSERT(extension->CurrentWriteRequest);
        ASSERT(stat->AmountInOutQueue >= extension->WriteLength);

     reqContext = SerialGetRequestContext(extension->CurrentWriteRequest);
        stat->AmountInOutQueue -= reqContext->Length - (extension->WriteLength);

    }

    stat->WaitForImmediate = extension->TransmitImmediate;

    stat->HoldReasons = 0;
    if (extension->TXHolding) {

        if (extension->TXHolding & SERIAL_TX_CTS) {

            stat->HoldReasons |= SERIAL_TX_WAITING_FOR_CTS;

        }

        if (extension->TXHolding & SERIAL_TX_DSR) {

            stat->HoldReasons |= SERIAL_TX_WAITING_FOR_DSR;

        }

        if (extension->TXHolding & SERIAL_TX_DCD) {

            stat->HoldReasons |= SERIAL_TX_WAITING_FOR_DCD;

        }

        if (extension->TXHolding & SERIAL_TX_XOFF) {

            stat->HoldReasons |= SERIAL_TX_WAITING_FOR_XON;

        }

        if (extension->TXHolding & SERIAL_TX_BREAK) {

            stat->HoldReasons |= SERIAL_TX_WAITING_ON_BREAK;

        }

    }

    if (extension->RXHolding & SERIAL_RX_DSR) {

        stat->HoldReasons |= SERIAL_RX_WAITING_FOR_DSR;

    }

    if (extension->RXHolding & SERIAL_RX_XOFF) {

        stat->HoldReasons |= SERIAL_TX_WAITING_XOFF_SENT;

    }

    return FALSE;
}

/*++

Routine Description:

    This is used to set the character that will be used to escape
    line status and modem status information when the application
    has set up that line status and modem status should be passed
    back in the data stream.

Arguments:

    Context - Pointer to the request that is specify the escape character.
              Implicitly - An escape character of 0 means no escaping
              will occur.

Return Value:

    This routine always returns FALSE.

--*/
_Use_decl_annotations_
BOOLEAN
SerialSetEscapeChar(
    WDFINTERRUPT Interrupt,
    PVOID Context
    )
{
    PREQUEST_CONTEXT reqContext = (PREQUEST_CONTEXT)Context;
    PSERIAL_DEVICE_EXTENSION extension = SerialGetDeviceExtension(WdfInterruptGetDevice(Interrupt));

    UNREFERENCED_PARAMETER(Interrupt);

    extension->EscapeChar = *(PUCHAR)reqContext->SystemBuffer;

    return FALSE;
}

/*++

Routine Description:

    This routine provides the initial processing for all of the
    Ioctrls for the serial device.

Arguments:

    Request - Pointer to the WDFREQUEST for the current request

Return Value:

    none

--*/
_Use_decl_annotations_
VOID
SerialEvtIoDeviceControl(
     WDFQUEUE Queue,
     WDFREQUEST Request,
     size_t OutputBufferLength,
     size_t InputBufferLength,
     ULONG IoControlCode
    )
{
    NTSTATUS status;
    PSERIAL_DEVICE_EXTENSION extension = NULL;

    PVOID buffer;
    PREQUEST_CONTEXT reqContext;
    size_t  bufSize;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    reqContext = SerialGetRequestContext(Request);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "++SerialEvtIoDeviceControl(%p)\r\n", Request);

    extension = SerialGetDeviceExtension(WdfIoQueueGetDevice(Queue));

    // We expect to be open so all our pages are locked down.  This is, after
    // all, an IO operation, so the device should be open first.

    if (extension->DeviceIsOpened != TRUE) {
       SerialCompleteRequest(Request, STATUS_INVALID_DEVICE_REQUEST, 0);
       return;
    }

    if (SerialCompleteIfError(extension, Request) != STATUS_SUCCESS) {

       TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                    "--SerialEvtIoDeviceControl (2) %d\r\n", STATUS_CANCELLED);
       return;
    }

    reqContext = SerialGetRequestContext(Request);
    reqContext->Information = 0;
    reqContext->Status = STATUS_SUCCESS;
    reqContext->MajorFunction = IRP_MJ_DEVICE_CONTROL;

    status = STATUS_SUCCESS;

    switch (IoControlCode) {

        case IOCTL_SERIAL_SET_BAUD_RATE : {

            ULONG baudRate;

            // Will hold the value of the appropriate divisor for
            // the requested baud rate.  If the baudrate is invalid
            // (because the device won't support that baud rate) then
            // this value is undefined.
            //
            // Note: in one sense the concept of a valid baud rate
            // is cloudy.  We could allow the user to request any
            // baud rate.  We could then calculate the divisor needed
            // for that baud rate.  As long as the divisor wasn't less
            // than one we would be "ok".  (The percentage difference
            // between the "true" divisor and the "rounded" value given
            // to the hardware might make it unusable, but... )  It would
            // really be up to the user to "Know" whether the baud rate
            // is suitable.  So much for theory, *We* only support a given
            // set of baud rates.

            SHORT appropriateDivisor;

            status = WdfRequestRetrieveInputBuffer (Request, sizeof(SERIAL_BAUD_RATE), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request memory"
                    " buffer %X\r\n", status);
                break;
            }

            baudRate = ((PSERIAL_BAUD_RATE)(buffer))->BaudRate;

            // Get the baud rate from the request.  We pass it
            // to a routine which will set the correct divisor.

            status = SerialGetDivisorFromBaud(extension->ClockRate,
                                             baudRate,
                                             &appropriateDivisor);

            if (NT_SUCCESS(status)) {

                SERIAL_IOCTL_SYNC serSync;

                extension->CurrentBaud = baudRate;
                extension->WmiCommData.BaudRate = baudRate;

                serSync.Extension = extension;
                serSync.Data = (PVOID)(ULONG_PTR)appropriateDivisor;

                WdfInterruptSynchronize(extension->WdfInterrupt,
                                        SerialSetBaud,
                                        &serSync);
            }

            break;
        }

        case IOCTL_SERIAL_GET_BAUD_RATE: {

            PSERIAL_BAUD_RATE br;

            status = WdfRequestRetrieveOutputBuffer (Request, sizeof(SERIAL_BAUD_RATE),
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request memory"
                    " buffer %X\r\n", status);
                break;
            }

            br = (PSERIAL_BAUD_RATE)buffer;

            br->BaudRate = extension->CurrentBaud;

            reqContext->Information = sizeof(SERIAL_BAUD_RATE);

            break;

        }

        case IOCTL_SERIAL_GET_MODEM_CONTROL: {
            SERIAL_IOCTL_SYNC serIoSync;

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(ULONG), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request memory"
                    " buffer %X\r\n", status);
                break;
            }

            reqContext->Information = sizeof(ULONG);

            serIoSync.Extension = extension;
            serIoSync.Data = buffer;

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialGetMCRContents,
                                    &serIoSync);

            break;
        }
        case IOCTL_SERIAL_SET_MODEM_CONTROL: {
            SERIAL_IOCTL_SYNC serIoSync;

            status = WdfRequestRetrieveInputBuffer(Request, sizeof(ULONG),
                                                    &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request "
                    "memory buffer %X\r\n", status);
                break;
            }

            serIoSync.Extension = extension;
            serIoSync.Data = buffer;

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialSetMCRContents,
                                    &serIoSync);

            break;
        }
        case IOCTL_SERIAL_SET_FIFO_CONTROL: {
            SERIAL_IOCTL_SYNC serIoSync;

            status = WdfRequestRetrieveInputBuffer (Request, sizeof(ULONG), &buffer, 
                &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request "
                    "memory buffer %X\r\n", status);
                break;
            }

            serIoSync.Extension = extension;
            serIoSync.Data = buffer;

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialSetFCRContents,
                                    &serIoSync);

            break;
        }
        case IOCTL_SERIAL_SET_LINE_CONTROL: {

            PSERIAL_LINE_CONTROL lc;
            UCHAR lData;
            UCHAR lStop;
            UCHAR lParity;
            UCHAR Mask = 0xff;

            status = WdfRequestRetrieveInputBuffer (Request, sizeof(SERIAL_LINE_CONTROL), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request"
                    " memory buffer %X\r\n", status);
                break;
            }

            // Points to the line control record in the Request.

            lc =  (PSERIAL_LINE_CONTROL)buffer;

            switch (lc->WordLength) {
                case 7: {

                    lData = SERIAL_7_DATA;
                    Mask = 0x7f;
                    break;

                }
                case 8: {

                    lData = SERIAL_8_DATA;
                    break;

                }
                default: {

                    status = STATUS_INVALID_PARAMETER;
                    goto DoneWithIoctl;

                }

            }

            extension->WmiCommData.BitsPerByte = lc->WordLength;

            switch (lc->Parity) {

                case NO_PARITY: {
                    extension->WmiCommData.Parity = SERIAL_WMI_PARITY_NONE;
                    lParity = SERIAL_NONE_PARITY;
                    break;

                }
                case EVEN_PARITY: {
                    extension->WmiCommData.Parity = SERIAL_WMI_PARITY_EVEN;
                    lParity = SERIAL_EVEN_PARITY;
                    break;

                }
                case ODD_PARITY: {
                    extension->WmiCommData.Parity = SERIAL_WMI_PARITY_ODD;
                    lParity = SERIAL_ODD_PARITY;
                    break;

                }
                case SPACE_PARITY: {
                    extension->WmiCommData.Parity = SERIAL_WMI_PARITY_SPACE;
                    lParity = SERIAL_SPACE_PARITY;
                    break;

                }
                case MARK_PARITY: {
                    extension->WmiCommData.Parity = SERIAL_WMI_PARITY_MARK;
                    lParity = SERIAL_MARK_PARITY;
                    break;

                }
                default: {

                    status = STATUS_INVALID_PARAMETER;
                    goto DoneWithIoctl;
                    break;
                }

            }

            switch (lc->StopBits) {

                case STOP_BIT_1: {
                    extension->WmiCommData.StopBits = SERIAL_WMI_STOP_1;
                    lStop = SERIAL_1_STOP;
                    break;
                }

                default: {

                    status = STATUS_INVALID_PARAMETER;
                    goto DoneWithIoctl;
                }

            }

            extension->LineControl =
                (UCHAR)((extension->LineControl & SERIAL_LCR_BREAK) |
                        (lData | lParity | lStop));
            extension->ValidDataMask = Mask;

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialSetLineControl,
                                    extension);

            break;
        }
        case IOCTL_SERIAL_GET_LINE_CONTROL: {

            PSERIAL_LINE_CONTROL lc;

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(SERIAL_LINE_CONTROL), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request"
                    " memory buffer %X\r\n", status);
                break;
            }

            lc = (PSERIAL_LINE_CONTROL)buffer;

            RtlZeroMemory(buffer, OutputBufferLength);

           if ((extension->LineControl & SERIAL_DATA_MASK)
                        == SERIAL_7_DATA) {
                lc->WordLength = 7;
            } else if ((extension->LineControl & SERIAL_DATA_MASK)
                        == SERIAL_8_DATA) {
                lc->WordLength = 8;
            }

            if ((extension->LineControl & SERIAL_PARITY_MASK)
                    == SERIAL_NONE_PARITY) {
                lc->Parity = NO_PARITY;
            } else if ((extension->LineControl & SERIAL_PARITY_MASK)
                    == SERIAL_ODD_PARITY) {
                lc->Parity = ODD_PARITY;
            } else if ((extension->LineControl & SERIAL_PARITY_MASK)
                    == SERIAL_EVEN_PARITY) {
                lc->Parity = EVEN_PARITY;
            } else if ((extension->LineControl & SERIAL_PARITY_MASK)
                    == SERIAL_MARK_PARITY) {
                lc->Parity = MARK_PARITY;
            } else if ((extension->LineControl & SERIAL_PARITY_MASK)
                    == SERIAL_SPACE_PARITY) {
                lc->Parity = SPACE_PARITY;
            }

            lc->StopBits = STOP_BIT_1;

            reqContext->Information = sizeof(SERIAL_LINE_CONTROL);

            break;
        }
        case IOCTL_SERIAL_SET_TIMEOUTS: {

            PSERIAL_TIMEOUTS newTimeouts;

            status = WdfRequestRetrieveInputBuffer(Request, sizeof(SERIAL_TIMEOUTS),
                                             &buffer, &bufSize);

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request"
                    " memory buffer %X\r\n", status);
                break;
            }

            newTimeouts =(PSERIAL_TIMEOUTS)buffer;

            if ((newTimeouts->ReadIntervalTimeout == MAXULONG) &&
                (newTimeouts->ReadTotalTimeoutMultiplier == MAXULONG) &&
                (newTimeouts->ReadTotalTimeoutConstant == MAXULONG)) {

                status = STATUS_INVALID_PARAMETER;
                break;

            }

            extension->timeouts.ReadIntervalTimeout =
                newTimeouts->ReadIntervalTimeout;

            extension->timeouts.ReadTotalTimeoutMultiplier =
                newTimeouts->ReadTotalTimeoutMultiplier;

            extension->timeouts.ReadTotalTimeoutConstant =
                newTimeouts->ReadTotalTimeoutConstant;

            extension->timeouts.WriteTotalTimeoutMultiplier =
                newTimeouts->WriteTotalTimeoutMultiplier;

            extension->timeouts.WriteTotalTimeoutConstant =
                newTimeouts->WriteTotalTimeoutConstant;

            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, 
                "SET_TIMEOUTS read: Intrv=%u, TotMul=%lu, TotConst=%lu; write: TotalInterv mul=%lu,"
                " const=%lu\r\n",
                extension->timeouts.ReadIntervalTimeout,
                extension->timeouts.ReadTotalTimeoutMultiplier,
                extension->timeouts.ReadTotalTimeoutConstant,
                extension->timeouts.WriteTotalTimeoutMultiplier,
                extension->timeouts.WriteTotalTimeoutConstant);
            break;
        }
        case IOCTL_SERIAL_GET_TIMEOUTS: {

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(SERIAL_TIMEOUTS), &buffer, 
                &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request memory buffer"
                    " %X\r\n", status);
                break;
            }

            *((PSERIAL_TIMEOUTS)buffer) = extension->timeouts;
            reqContext->Information = sizeof(SERIAL_TIMEOUTS);

            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, 
                "GET_TIMEOUTS read: Intrv=%u, TotMul=%lu, TotConst=%lu; write: TotalInterv mul=%lu,"
                " const=%lu\r\n",
                extension->timeouts.ReadIntervalTimeout,
                extension->timeouts.ReadTotalTimeoutMultiplier,
                extension->timeouts.ReadTotalTimeoutConstant,
                extension->timeouts.WriteTotalTimeoutMultiplier,
                extension->timeouts.WriteTotalTimeoutConstant);
            break;
        }
        case IOCTL_SERIAL_SET_CHARS: {

            SERIAL_IOCTL_SYNC serSync;
            PSERIAL_CHARS newChars;

           status = WdfRequestRetrieveInputBuffer ( Request, sizeof(SERIAL_CHARS), &buffer, &bufSize );
            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request memory buffer %X\r\n", 
                        status);
                break;
            }

            newChars = (PSERIAL_CHARS)buffer;

            // We acquire the control lock so that only
            // one request can GET or SET the characters
            // at a time.  The sets could be synchronized
            // by the interrupt spinlock, but that wouldn't
            // prevent multiple gets at the same time.

            serSync.Extension = extension;
            serSync.Data = newChars;

            // Under the protection of the lock, make sure that
            // the xon and xoff characters aren't the same as
            // the escape character.
            //

            if (extension->EscapeChar) {

                if ((extension->EscapeChar == newChars->XonChar) ||
                    (extension->EscapeChar == newChars->XoffChar)) {

                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
            }

            extension->WmiCommData.XonCharacter = newChars->XonChar;
            extension->WmiCommData.XoffCharacter = newChars->XoffChar;

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialSetChars,
                                    &serSync);

            break;

        }
        case IOCTL_SERIAL_GET_CHARS: {

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(SERIAL_CHARS), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request memory"
                    " buffer %X\r\n", status);
                break;
            }

            *((PSERIAL_CHARS)buffer) = extension->SpecialChars;
            reqContext->Information = sizeof(SERIAL_CHARS);

            break;
        }
        case IOCTL_SERIAL_SET_DTR:
        case IOCTL_SERIAL_CLR_DTR: {

            // We acquire the lock so that we can check whether
            // automatic dtr flow control is enabled.  If it is
            // then we return an error since the app is not allowed
            // to touch this if it is automatic.

            if ((extension->HandFlow.ControlHandShake & SERIAL_DTR_MASK)
                == SERIAL_DTR_HANDSHAKE) {

                status = STATUS_INVALID_PARAMETER;

            } else {

                WdfInterruptSynchronize(extension->WdfInterrupt,
                                        ((IoControlCode ==
                                         IOCTL_SERIAL_SET_DTR)?
                                         (SerialSetDTR):(SerialClrDTR)),
                                        extension);
            }

            break;
        }
        case IOCTL_SERIAL_RESET_DEVICE: {

            break;
        }
        case IOCTL_SERIAL_SET_RTS:
        case IOCTL_SERIAL_CLR_RTS: {

            // We acquire the lock so that we can check whether
            // automatic rts flow control or transmit toggleing
            // is enabled.  If it is then we return an error since
            // the app is not allowed to touch this if it is automatic
            // or toggling.

            if (((extension->HandFlow.FlowReplace & SERIAL_RTS_MASK)
                 == SERIAL_RTS_HANDSHAKE) ||
                ((extension->HandFlow.FlowReplace & SERIAL_RTS_MASK)
                 == SERIAL_TRANSMIT_TOGGLE)) {

                status = STATUS_INVALID_PARAMETER;

            } else {

                WdfInterruptSynchronize(extension->WdfInterrupt,
                                        ((IoControlCode ==
                                        IOCTL_SERIAL_SET_RTS)?
                                        (SerialSetRTS):(SerialClrRTS)),
                                        extension);
            }

            break;

        }
        case IOCTL_SERIAL_SET_XOFF: {

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialPretendXoff,
                                    extension);

            break;

        }
        case IOCTL_SERIAL_SET_XON: {

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialPretendXon,
                                    extension);

            break;

        }
        case IOCTL_SERIAL_SET_BREAK_ON: {

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialTurnOnBreak,
                                    extension);

            break;
        }
        case IOCTL_SERIAL_SET_BREAK_OFF: {

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialTurnOffBreak,
                                    extension);

            break;
        }
        case IOCTL_SERIAL_SET_QUEUE_SIZE: {

            // Type ahead buffer is fixed, so we just validate
            // the the users request is not bigger that our
            // own internal buffer size.

            PSERIAL_QUEUE_SIZE rs;

            status = WdfRequestRetrieveInputBuffer ( Request, sizeof(SERIAL_QUEUE_SIZE), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "Could not get request "
                    "memory buffer %X\r\n", status);
                break;
            }

            ASSERT(extension->InterruptReadBuffer);

            rs = (PSERIAL_QUEUE_SIZE)buffer;

            reqContext->SystemBuffer = buffer;

            // We have to allocate the memory for the new
            // buffer while we're still in the context of the
            // caller.  We don't even try to protect this
            // with a lock because the value could be stale
            // as soon as we release the lock - The only time
            // we will know for sure is when we actually try
            // to do the resize.

            if (rs->InSize <= extension->BufferSize) {

                status = STATUS_SUCCESS;
                break;
            }

            reqContext->Type3InputBuffer =
                    ExAllocatePoolWithQuotaTag(NonPagedPool | POOL_QUOTA_FAIL_INSTEAD_OF_RAISE,
                                                rs->InSize,
                                                POOL_TAG);

            if (!reqContext->Type3InputBuffer) {

                status = STATUS_INSUFFICIENT_RESOURCES;
                break;

            }

            // Well the data passed was big enough.  Do the request.
            //
            // There are two reason we place it in the read queue:
            //
            // 1) We want to serialize these resize requests so that
            //    they don't contend with each other.
            //
            // 2) We want to serialize these requests with reads since
            //    we don't want reads and resizes contending over the
            //    read buffer.

            SerialStartOrQueue(extension,
                                Request,
                                extension->ReadQueue,
                                &extension->CurrentReadRequest,
                                SerialStartRead);

            return;
        }
        case IOCTL_SERIAL_GET_WAIT_MASK: {

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(ULONG), &buffer, &bufSize );
            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            // Simple scalar read.  No reason to acquire a lock.

            reqContext->Information = sizeof(ULONG);

            *((ULONG*)buffer) = extension->IsrWaitMask;

            break;

        }
        case IOCTL_SERIAL_SET_WAIT_MASK: {

            ULONG NewMask;

            TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "In Ioctl processing for set mask\r\n");

            status = WdfRequestRetrieveInputBuffer ( Request, sizeof(ULONG), &buffer, &bufSize );
            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            NewMask = *((ULONG*)buffer);
            reqContext->SystemBuffer = buffer;

            // Make sure that the mask only contains valid
            // waitable events.

            if (NewMask & ~(SERIAL_EV_RXCHAR   |
                            SERIAL_EV_RXFLAG   |
                            SERIAL_EV_TXEMPTY  |
                            SERIAL_EV_CTS      |
                            SERIAL_EV_DSR      |
                            SERIAL_EV_RLSD     |
                            SERIAL_EV_BREAK    |
                            SERIAL_EV_ERR      |
                            SERIAL_EV_RING     |
                            SERIAL_EV_PERR     |
                            SERIAL_EV_RX80FULL |
                            SERIAL_EV_EVENT1   |
                            SERIAL_EV_EVENT2)) {

                TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "Unknown mask %x\r\n", NewMask);

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            // Either start this request or put it on the
            // queue.

            TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS,
                        "Starting or queuing set mask request %p\r\n",
                        Request);

            SerialStartOrQueue(extension,
                            Request,
                            extension->MaskQueue,
                            &extension->CurrentMaskRequest,
                            SerialStartMask);
            return;

        }
        case IOCTL_SERIAL_WAIT_ON_MASK: {

            TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "In Ioctl processing for wait mask\r\n");

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(ULONG), &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            reqContext->SystemBuffer = buffer;

            // Either start this request or put it on the
            // queue.

            TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS,
                        "Starting or queuing wait mask request %p\r\n", 
                        Request);

            SerialStartOrQueue(extension,
                               Request,
                               extension->MaskQueue,
                               &extension->CurrentMaskRequest,
                               SerialStartMask);
            return;
        }
        case IOCTL_SERIAL_IMMEDIATE_CHAR: {

            status = WdfRequestRetrieveInputBuffer ( Request, sizeof(UCHAR), &buffer, &bufSize );
            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            reqContext->SystemBuffer = buffer;

            if (extension->CurrentImmediateRequest) {

                status = STATUS_INVALID_PARAMETER;

            } else {

                // We can queue the char.  We need to set
                // a cancel routine because flow control could
                // keep the char from transmitting.  Make sure
                // that the request hasn't already been canceled.

                extension->CurrentImmediateRequest = Request;
                extension->TotalCharsQueued++;
                SerialStartImmediate(extension);
                return;
            }

            break;

        }
        case IOCTL_SERIAL_PURGE: {

            ULONG mask;

            status = WdfRequestRetrieveInputBuffer ( Request, sizeof(ULONG), &buffer, &bufSize );
            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            // Check to make sure that the mask only has
            // 0 or the other appropriate values.

            mask = *((ULONG*)(buffer));

            if ((!mask) || (mask & (~(SERIAL_PURGE_TXABORT |
                                      SERIAL_PURGE_RXABORT |
                                      SERIAL_PURGE_TXCLEAR |
                                      SERIAL_PURGE_RXCLEAR))
                           )) {

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            reqContext->SystemBuffer = buffer;

            // Either start this request or put it on the
            // queue.

            SerialStartOrQueue(extension,
                                Request,
                                extension->PurgeQueue,
                                &extension->CurrentPurgeRequest,
                                SerialStartPurge);
            return;
        }
        case IOCTL_SERIAL_GET_HANDFLOW: {

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(SERIAL_HANDFLOW), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            reqContext->Information = sizeof(SERIAL_HANDFLOW);

            *((PSERIAL_HANDFLOW)buffer) = extension->HandFlow;

            break;
        }
        case IOCTL_SERIAL_SET_HANDFLOW: {

            SERIAL_IOCTL_SYNC syn;
            PSERIAL_HANDFLOW handFlow;

            // Make sure that the hand shake and control is the
            // right size.

            status = WdfRequestRetrieveInputBuffer ( Request, sizeof(SERIAL_HANDFLOW), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            handFlow = (PSERIAL_HANDFLOW)buffer;

            // Make sure that there are no invalid bits set in
            // the control and handshake.

            if (handFlow->ControlHandShake & SERIAL_CONTROL_INVALID) {

                status = STATUS_INVALID_PARAMETER;
                break;

            }

            if (handFlow->FlowReplace & SERIAL_FLOW_INVALID) {

                status = STATUS_INVALID_PARAMETER;
                break;

            }

            // Make sure that the app hasn't set an invlid DTR mode.

            if ((handFlow->ControlHandShake & SERIAL_DTR_MASK) ==
                SERIAL_DTR_MASK) {

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            // Make sure that haven't set totally invalid xon/xoff
            // limits.

            if ((handFlow->XonLimit < 0) ||
                ((ULONG)handFlow->XonLimit > extension->BufferSize)) {

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            if ((handFlow->XoffLimit < 0) ||
                ((ULONG)handFlow->XoffLimit > extension->BufferSize)) {

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            syn.Extension = extension;
            syn.Data = handFlow;

            // Under the protection of the lock, make sure that
            // we aren't turning on error replacement when we
            // are doing line status/modem status insertion.

            if (extension->EscapeChar) {

                if (handFlow->FlowReplace & SERIAL_ERROR_CHAR) {

                    status = STATUS_INVALID_PARAMETER;
                    break;

                }

            }

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialSetHandFlow,
                                    &syn);

            break;
        }
        case IOCTL_SERIAL_GET_MODEMSTATUS: {

            SERIAL_IOCTL_SYNC syn;

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(ULONG), &buffer, 
                &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            reqContext->Information = sizeof(ULONG);

            syn.Extension = extension;
            syn.Data = buffer;

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialGetModemUpdate,
                                    &syn);

            break;

        }
        case IOCTL_SERIAL_GET_DTRRTS: {

            ULONG modemControl;

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(ULONG), &buffer, &bufSize );
            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            reqContext->Information = sizeof(ULONG);
            reqContext->Status = STATUS_SUCCESS;

            // Reading this hardware has no effect on the device.

            modemControl = READ_MODEM_CONTROL(extension, extension->Controller);

            modemControl &= SERIAL_DTR_STATE | SERIAL_RTS_STATE;

            *(PULONG)buffer = modemControl;

            break;

        }
        case IOCTL_SERIAL_GET_COMMSTATUS: {

            SERIAL_IOCTL_SYNC syn;

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(SERIAL_STATUS), &buffer, 
                &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            reqContext->Information = sizeof(SERIAL_STATUS);

            syn.Extension = extension;
            syn.Data =  buffer;

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialGetCommStatus,
                                    &syn);

            break;
        }
        case IOCTL_SERIAL_GET_PROPERTIES: {

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(SERIAL_COMMPROP), &buffer, 
                &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            // No synchronization is required since this information
            // is "static".

            SerialGetProperties(extension,
                                buffer);

            reqContext->Information = sizeof(SERIAL_COMMPROP);
            reqContext->Status = STATUS_SUCCESS;

            break;
        }
        case IOCTL_SERIAL_XOFF_COUNTER: {

            PSERIAL_XOFF_COUNTER xc;

            status = WdfRequestRetrieveInputBuffer ( Request, sizeof(SERIAL_XOFF_COUNTER), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            xc = (PSERIAL_XOFF_COUNTER)buffer;

            if (xc->Counter <= 0) {

                status = STATUS_INVALID_PARAMETER;
                break;

            }
            reqContext->SystemBuffer = buffer;

            // There is no output, so make that clear now

            reqContext->Information = 0;

            // So far so good.  Put the request onto the write queue.

            SerialStartOrQueue(extension,
                               Request,
                               extension->WriteQueue,
                               &extension->CurrentWriteRequest,
                               SerialStartWrite);
            return;

        }
        case IOCTL_SERIAL_LSRMST_INSERT: {

            PUCHAR escapeChar;
            SERIAL_IOCTL_SYNC syn;

            // Make sure we get a byte.

            status = WdfRequestRetrieveInputBuffer ( Request, sizeof(UCHAR), &buffer, 
                &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            reqContext->SystemBuffer = buffer;

            escapeChar = (PUCHAR)buffer;

            if (*escapeChar) {

                // We've got some escape work to do.  We will make sure that
                // the character is not the same as the Xon or Xoff character,
                // or that we are already doing error replacement.

                if ((*escapeChar == extension->SpecialChars.XoffChar) ||
                    (*escapeChar == extension->SpecialChars.XonChar) ||
                    (extension->HandFlow.FlowReplace & SERIAL_ERROR_CHAR)) {

                    status = STATUS_INVALID_PARAMETER;

                    break;
                }
            }

            syn.Extension = extension;
            syn.Data = buffer;

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialSetEscapeChar,
                                    reqContext);

            break;

        }
        case IOCTL_SERIAL_CONFIG_SIZE: {

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(ULONG), &buffer, 
                &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            reqContext->Information = sizeof(ULONG);
            reqContext->Status = STATUS_SUCCESS;

            *(PULONG)buffer = 0;

            break;
        }
        case IOCTL_SERIAL_GET_STATS: {

            status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(SERIALPERF_STATS), 
                &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

            reqContext->SystemBuffer = buffer;

            reqContext->Information = sizeof(SERIALPERF_STATS);
            reqContext->Status = STATUS_SUCCESS;

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialGetStats,
                                    reqContext);

            break;
        }
        case IOCTL_SERIAL_CLEAR_STATS: {

            WdfInterruptSynchronize(extension->WdfInterrupt,
                                    SerialClearStats,
                                    extension);
            break;
        }
        default: {

            status = STATUS_INVALID_PARAMETER;
            break;
        }
    }

DoneWithIoctl:

    reqContext->Status = status;
    SerialCompleteRequest(Request, status, reqContext->Information);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "--SerialEvtIoDeviceControl(%p)="
        "%Xh\r\n", Request,status);

    return;
}

/*++

Routine Description:

    This function returns the capabilities of this particular
    serial device.

Arguments:

    Extension - The serial device extension.

    Properties - The structure used to return the properties

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialGetProperties(
     PSERIAL_DEVICE_EXTENSION Extension,
     PSERIAL_COMMPROP Properties
    )
{
    RtlZeroMemory(
        Properties,
        sizeof(SERIAL_COMMPROP));

    Properties->PacketLength = sizeof(SERIAL_COMMPROP);
    Properties->PacketVersion = 2;
    Properties->ServiceMask = SERIAL_SP_SERIALCOMM;
    Properties->MaxTxQueue = 0;
    Properties->MaxRxQueue = 0;

    Properties->MaxBaud = SERIAL_BAUD_USER;
    Properties->SettableBaud = Extension->SupportedBauds;

    Properties->ProvSubType = SERIAL_SP_RS232;
    Properties->ProvCapabilities = SERIAL_PCF_PARITY_CHECK |
                                   SERIAL_PCF_XONXOFF |
                                   SERIAL_PCF_SETXCHAR ;
                                   SERIAL_PCF_TOTALTIMEOUTS |
                                   SERIAL_PCF_INTTIMEOUTS;
    Properties->SettableParams = SERIAL_SP_PARITY |
                                 SERIAL_SP_BAUD |
                                 SERIAL_SP_DATABITS |
                                 SERIAL_SP_STOPBITS ;

    Properties->SettableData = SERIAL_DATABITS_7 |
                               SERIAL_DATABITS_8;

    Properties->SettableStopParity = SERIAL_STOPBITS_10 |
                                     SERIAL_PARITY_NONE ;
    Properties->CurrentTxQueue = 0;
    Properties->CurrentRxQueue = Extension->BufferSize;
}

/*++

Routine Description:

    This routine provides the initial processing for all of the
    internal Ioctrls for the serial device.

Arguments:

    PDevObj - Pointer to the device object for this device

    PIrp - Pointer to the WDFREQUEST for the current request

Return Value:

    The function value is the final status of the call

--*/
_Use_decl_annotations_
VOID
SerialEvtIoInternalDeviceControl (
     WDFQUEUE Queue,
     WDFREQUEST Request,
     size_t OutputBufferLength,
     size_t InputBufferLength,
     ULONG IoControlCode
)
{
    NTSTATUS status;
    PSERIAL_DEVICE_EXTENSION pDevExt;
    PVOID buffer;
    PREQUEST_CONTEXT reqContext;
    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;
    size_t  bufSize;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS,
                "++SerialEvtIoInternalDeviceControl(req=%ph, IOCtrlCode=%Xh)\r\n",
                Request,
                IoControlCode);

    pDevExt = SerialGetDeviceExtension(WdfIoQueueGetDevice(Queue));

    if (SerialCompleteIfError(pDevExt, Request) != STATUS_SUCCESS) {

       TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
                    "--SerialEvtIoDeviceControl()=%Xh\r\n",
                    (ULONG)STATUS_CANCELLED);
       return;
    }

    reqContext = SerialGetRequestContext(Request);
    reqContext->Information = 0;
    reqContext->Status = STATUS_SUCCESS;
    reqContext->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;

    switch (IoControlCode) {

    case IOCTL_SERIAL_INTERNAL_DO_WAIT_WAKE:

        // Init wait-wake policy structure.

        WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);

        // Override the default settings from allow user control to do not allow.

        wakeSettings.UserControlOfWakeSettings = IdleDoNotAllowUserControl;
        status = WdfDeviceAssignSxWakeSettings(pDevExt->WdfDevice, &wakeSettings);

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                        "WdfDeviceAssignSxWakeSettings failed %Xh\r\n",
                        status);
            break;
        }

       pDevExt->IsWakeEnabled = TRUE;
       status = STATUS_SUCCESS;
       break;

    case IOCTL_SERIAL_INTERNAL_CANCEL_WAIT_WAKE:

       WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);

       // Override the default settings.
       // Disable wait-wake

       wakeSettings.Enabled = WdfFalse;
       wakeSettings.UserControlOfWakeSettings = IdleDoNotAllowUserControl;
       status = WdfDeviceAssignSxWakeSettings(pDevExt->WdfDevice, &wakeSettings);

       if (!NT_SUCCESS(status)) {
           TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                        "WdfDeviceAssignSxWakeSettings failed %Xh\r\n",
                        status);
           break;
       }

       pDevExt->IsWakeEnabled = FALSE;
       status = STATUS_SUCCESS;
       break;

    // Put the serial port in a "filter-driver" appropriate state
    //
    // WARNING: This code assumes it is being called by a trusted kernel
    // entity and no checking is done on the validity of the settings
    // passed to IOCTL_SERIAL_INTERNAL_RESTORE_SETTINGS
    //
    // If validity checking is desired, the regular ioctl's should be used

    case IOCTL_SERIAL_INTERNAL_BASIC_SETTINGS:
    case IOCTL_SERIAL_INTERNAL_RESTORE_SETTINGS: {

       SERIAL_BASIC_SETTINGS   basic;
       PSERIAL_BASIC_SETTINGS  pBasic;
       SERIAL_IOCTL_SYNC       s;

       if (IoControlCode == IOCTL_SERIAL_INTERNAL_BASIC_SETTINGS) {

         // Check the buffer size

         status = WdfRequestRetrieveOutputBuffer ( Request, sizeof(SERIAL_BASIC_SETTINGS), 
            &buffer, &bufSize );

         if( !NT_SUCCESS(status) ) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                        "Could not get request memory buffer %X\r\n",
                        status);
            break;
         }

         reqContext->SystemBuffer = buffer;

          // Everything is 0 -- timeouts and flow control and fifos.  If
          // We add additional features, this zero memory method
          // may not work.

          RtlZeroMemory(&basic, sizeof(SERIAL_BASIC_SETTINGS));

          basic.TxFifo = 1;
          basic.RxFifo = SERIAL_1_BYTE_HIGH_WATER;

          reqContext->Information = sizeof(SERIAL_BASIC_SETTINGS);
          pBasic = (PSERIAL_BASIC_SETTINGS)buffer;

          // Save off the old settings

          RtlCopyMemory(&pBasic->Timeouts, &pDevExt->timeouts,
                        sizeof(SERIAL_TIMEOUTS));

          RtlCopyMemory(&pBasic->HandFlow, &pDevExt->HandFlow,
                        sizeof(SERIAL_HANDFLOW));

          pBasic->RxFifo = pDevExt->RxFifoTrigger;
          pBasic->TxFifo = pDevExt->TxFifoAmount;

          // Point to our new settings

          pBasic = &basic;
       } else {

            // restoring settings

            status = WdfRequestRetrieveInputBuffer ( Request, sizeof(SERIAL_BASIC_SETTINGS), 
            &buffer, &bufSize );

            if( !NT_SUCCESS(status) ) {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
                            "Could not get request memory buffer %X\r\n",
                            status);
                break;
            }

          pBasic = (PSERIAL_BASIC_SETTINGS)buffer;
       }

       // Set the timeouts

       RtlCopyMemory(&pDevExt->timeouts, &pBasic->Timeouts,
                     sizeof(SERIAL_TIMEOUTS));

       // Set flowcontrol

       s.Extension = pDevExt;
       s.Data = &pBasic->HandFlow;
       WdfInterruptSynchronize(pDevExt->WdfInterrupt, SerialSetHandFlow, &s);

       if (pDevExt->FifoPresent) {
          pDevExt->TxFifoAmount = pBasic->TxFifo;
          pDevExt->RxFifoTrigger = (UCHAR)pBasic->RxFifo;

          WRITE_FIFO_CONTROL(pDevExt, pDevExt->Controller, (UCHAR)0);
          READ_RECEIVE_BUFFER(pDevExt, pDevExt->Controller);
          WRITE_FIFO_CONTROL(pDevExt, pDevExt->Controller,
                             (UCHAR)(SERIAL_FCR_ENABLE | pDevExt->RxFifoTrigger
                                     | SERIAL_FCR_RCVR_RESET
                                     | SERIAL_FCR_TXMT_RESET));
       } else {
          pDevExt->TxFifoAmount = pDevExt->RxFifoTrigger = 0;
          WRITE_FIFO_CONTROL(pDevExt, pDevExt->Controller, (UCHAR)0);
       }
       break;
    }

    default:
       status = STATUS_INVALID_PARAMETER;
       break;

    }

    reqContext->Status = status;

    SerialCompleteRequest(Request, reqContext->Status, reqContext->Information);

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "--SerialEvtIoInternalDeviceControl(req=%ph,"
        " IOCtrlCode=%Xh)=%Xh\r\n", Request,IoControlCode,status);

    return;
}



