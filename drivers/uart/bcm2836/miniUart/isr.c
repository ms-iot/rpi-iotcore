// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    isr.c
//
// Abstract:
//
//   This module contains the interrupt service routine for
//   Pi mini Uart serial driver.
//

#include "precomp.h"
#include "isr.tmh"

// If you need to fine tune WPP tracing in ISR, an additional compile time macro
// helps to control WPP output in selected parts by turning on and off tracing flag
// for sub components selected by DBG_INTERRUPT flag

#define TRACE_LEVEL_ISROUTP TRACE_LEVEL_VERBOSE

// *********************** IMPORTANT 16550 UART COMPATIBILITY NOTICE *************************************
// Please note that miniUart hardware has limited compatibility with 16C550 like UART device registers.
// Important information below regarding IER_REG register
//
// from BCM2835 data sheet when DLAB=0
//
// bit 0 - transmit interrupt (interrupt line is asserted whenever the transmit FIFO is empty)
// bit 1 - receive interrupt (interrupt line is asserted whenever the receive FIFO holds at least 1 byte)
// bits 7:2 - Reserved, write zero, read as don’t care
//
// from BCM2835 Errata
//
// * Bits 1:0 are swapped. *
// bit 0 - receive interrupt
// bit 1 - transmit interrupt
// bits 3:2 - may have to be set
// *****************************************************************************************************

/*++

Routine Description:

    This event is called when the Framework moves the device to D0, and after
    EvtDeviceD0Entry.  The driver should enable its interrupt here.

    This function will be called at the device's assigned interrupt
    IRQL (DIRQL.)

    RPi3 mini Uart driver does not do anything here since it will control inetrrupts
    in different places. Please see open, close and isr functions.

Arguments:

    Interrupt - Handle to a Framework interrupt object.

    AssociatedDevice - Handle to a Framework device object.

Return Value:

    BOOLEAN - TRUE indicates that the interrupt was successfully enabled.

--*/
_Use_decl_annotations_
NTSTATUS
SerialEvtInterruptEnable(
     WDFINTERRUPT Interrupt,
     WDFDEVICE AssociatedDevice
    )
{
    UNREFERENCED_PARAMETER(Interrupt);
    UNREFERENCED_PARAMETER(AssociatedDevice);

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "++SerialEvtInterruptEnable\r\n");

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "--SerialEvtInterruptEnable\r\n");

    return STATUS_SUCCESS;
}

/*++

Routine Description:

    This event is called before the Framework moves the device to D1, D2 or D3
    and before EvtDeviceD0Exit.  The driver should disable its interrupt here.

    This function will be called at the device's assigned interrupt
    IRQL (DIRQL.)

    RPi3 mini Uart driver does not do anything here since it will control inetrrupts
    in different places. Please see open, close and isr functions.

Arguments:

    Interrupt - Handle to a Framework interrupt object.

    AssociatedDevice - Handle to a Framework device object.

Return Value:

    BOOLEAN - TRUE indicates that the interrupt was successfully disabled.

--*/
_Use_decl_annotations_
NTSTATUS
SerialEvtInterruptDisable(
     WDFINTERRUPT Interrupt,
     WDFDEVICE AssociatedDevice
    )
{
    UNREFERENCED_PARAMETER(Interrupt);
    UNREFERENCED_PARAMETER(AssociatedDevice);

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "++SerialEvtInterruptDisable\r\n");

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "--SerialEvtInterruptDisable\r\n");

    return STATUS_SUCCESS;
}

/*++

Routine Description:

    This is the interrupt service routine for mini Uart driver.
    It will determine whether the serial port is the source of this
    interrupt.  If it is, then this routine will do the minimum of
    processing to quiet the interrupt.  It will store any information
    necessary for later processing.

Arguments:

    InterruptObject - Points to the interrupt object declared for this
    device.  We *do not* use this parameter.

Return Value:

    This function will return TRUE if the serial port is the source
    of this interrupt, FALSE otherwise.

--*/
_Use_decl_annotations_
BOOLEAN
SerialISR(
     WDFINTERRUPT Interrupt,
     ULONG MessageID
    )
{
    UCHAR receivedChar=0xFF;
    UCHAR uchIerTemp=0x00;
    static ULONG ulIsrCallCount=0;
    static ULONG ulIsrInnerLoopCnt=0;

    // Holds the information specific to handling this device.

    PSERIAL_DEVICE_EXTENSION extension = NULL;

    // Holds the contents of the interrupt identification record.
    // A low bit of zero in this register indicates that there is
    // an interrupt pending on this device.

    UCHAR interruptIdReg;

    // Will hold whether we've serviced any interrupt causes in this
    // routine.

    BOOLEAN servicedAnInterrupt;
    SHORT readFifoLvl=0;
    SHORT transmitFifoLvl=0;
    UCHAR tempLsr;
    PREQUEST_CONTEXT reqContext = NULL;
    INT32 iReadCnt=0;
    UCHAR uchAuxIrq=0x00;
    UNREFERENCED_PARAMETER(MessageID);

    TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                "++SerialISR(msg=%Xh) c=%lu\r\n",
                MessageID, 
                ulIsrCallCount);

    extension = SerialGetDeviceExtension(WdfInterruptGetDevice(Interrupt));

    // Make sure we have an interrupt pending.  If we do then
    // we need to make sure that the device is open.  If the
    // device isn't open or powered down then quiet the device.  Note that
    // if the device isn't opened when we enter this routine
    // it can't open while we're in it.

    // for miniUart we check aux interrupt status first

    uchAuxIrq=READ_AUXINTERRUPT_STATUS(extension, extension->Controller);
    interruptIdReg = READ_INTERRUPT_ID_REG(extension, extension->Controller);

    if((uchAuxIrq & 0x1) ==0x00)
    {
        // this is SPI interrupt, so pass it along. This is valid case.

        servicedAnInterrupt = FALSE;
        TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, "SerialISR - perhaps SPI interrupt.\r\n");
    }
    else if ((interruptIdReg & SERIAL_IIR_NO_INTERRUPT_PENDING))
    {
        // miniUart spurious interrupt 

        servicedAnInterrupt = FALSE;
        TraceEvents(TRACE_LEVEL_WARNING, DBG_INTERRUPT, "SerialISR - miniUart spurious interrupt.\r\n");
    } 
    else if (extension->DeviceIsOpened==FALSE ) 
    {

        // We got an interrupt with the device being closed or when the
        // device is supposed to be powered down.  This
        // is not unlikely with a serial device.  We just quietly
        // keep servicing the causes until it calms down.

        TraceEvents(TRACE_LEVEL_WARNING, DBG_INTERRUPT, 
                    "SerialISR [%lu] - miniUart interrupt"
                    " with device Closed\r\n",
                    ulIsrInnerLoopCnt);

        ulIsrInnerLoopCnt=0;
        servicedAnInterrupt = TRUE;
        do 
        {
            // leave only these interrupt bits which are supported in miniuart

            interruptIdReg=interruptIdReg & ~SERIAL_IIR_FIFOS_ENABLED;

            switch (interruptIdReg) 
            {
                case SERIAL_IIR_RDA:

                // perform one char read
                
                receivedChar = READ_RECEIVE_BUFFER(extension, extension->Controller);
                
                // Writing with bit 1 set to IER_REG will clear read FIFO
                
                WRITE_INTERRUPT_ID_REG(extension, extension->Controller,0x2);
                break;

                case SERIAL_IIR_THR:

                // Writing with bits 2 set to IER_REG will clear write FIFO
                
                WRITE_INTERRUPT_ID_REG(extension, extension->Controller,0x4);

                // Already clear from reading the iir.
                //
                // We want to keep close track of whether
                // the holding register is empty.

                extension->HoldingEmpty = TRUE;
                break;

                // the following bits if set are an error on mini Uart

                case SERIAL_IIR_RLS:
                case SERIAL_IIR_MS:
                default:
                TraceEvents(TRACE_LEVEL_ERROR, DBG_INTERRUPT, 
                            "SerialISR(no) [%lu] - invalid"
                            " IIReg=%02Xh\r\n",
                            ulIsrInnerLoopCnt,
                            interruptIdReg);

                ASSERT(FALSE);
                break;
            };

            ulIsrInnerLoopCnt+=1;

        } while (!((interruptIdReg =
                    READ_INTERRUPT_ID_REG(extension, extension->Controller))
                    & SERIAL_IIR_NO_INTERRUPT_PENDING));

    } 
    else 
    {
        ulIsrInnerLoopCnt=0;
        TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                    "SerialISR(o) [%lu] - miniUart"
                    " interrupt. IIReg=%02Xh\r\n",
                    ulIsrInnerLoopCnt, interruptIdReg);

        servicedAnInterrupt = TRUE;

        do {

            // We only care about bits that can denote miniuart interrupt.

            interruptIdReg &= SERIAL_IIR_RDA | SERIAL_IIR_THR ;

            // We have an interrupt.  We look for interrupt causes
            // in priority order.  The presence of a higher interrupt
            // will mask out causes of a lower priority.  When we service
            // and quiet a higher priority interrupt we then need to check
            // the interrupt causes to see if a new interrupt cause is
            // present.

            switch (interruptIdReg) 
            {

                case SERIAL_IIR_RDA:
                {
                    TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                "SerialISR(o) [%lu] - RCV miniUart interrupt, IIR=%02Xh\r\n",
                                ulIsrInnerLoopCnt,
                                interruptIdReg);

                    // Reading the receive buffer will quiet this interrupt.
                    //
                    // It may also reveal a new interrupt cause.
                    //

                    // RPi mini Uart can report how many chars are in read fifo: 
                    // bits 16-19 are Rx FIFO level

                    readFifoLvl=(SHORT)((READ_EXTRA_STATUS(extension->Controller) & 0x000F0000)>>16);

                    do {

                        receivedChar =
                            READ_RECEIVE_BUFFER(extension, extension->Controller);

                        TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                    "SerialISR [%lu] - recvd %02Xh byte. Rx FIFO lvl=%u\r\n",
                                    ulIsrInnerLoopCnt, 
                                    receivedChar,
                                    readFifoLvl); 

                        extension->PerfStats.ReceivedCount++;
                        extension->WmiPerfData.ReceivedCount++;

                        receivedChar &= extension->ValidDataMask;

                        if (!receivedChar &&
                            (extension->HandFlow.FlowReplace &
                             SERIAL_NULL_STRIPPING)) {

                            // If what we got is a null character
                            // and we're doing null stripping, then
                            // we simply act as if we didn't see it.

                            goto ReceiveDoLineStatus;
                        }

                        if ((extension->HandFlow.FlowReplace &
                             SERIAL_AUTO_TRANSMIT) &&
                            ((receivedChar ==
                              extension->SpecialChars.XonChar) ||
                             (receivedChar ==
                              extension->SpecialChars.XoffChar))) {

                            // No matter what happens this character
                            // will never get seen by the app.

                            if (receivedChar ==
                                extension->SpecialChars.XoffChar) {

                                extension->TXHolding |= SERIAL_TX_XOFF;

                                if ((extension->HandFlow.FlowReplace &
                                     SERIAL_RTS_MASK) ==
                                     SERIAL_TRANSMIT_TOGGLE) {

                                    SerialInsertQueueDpc(extension->StartTimerLowerRTSDpc)
                                        ?extension->CountOfTryingToLowerRTS++:0;
                                }

                            } else {

                                if (extension->TXHolding & SERIAL_TX_XOFF) {

                                    // We got the xon char **AND*** we
                                    // were being held up on transmission
                                    // by xoff.  Clear that we are holding
                                    // due to xoff.  Transmission will
                                    // automatically restart because of
                                    // the code outside the main loop 

                                    extension->TXHolding &= ~SERIAL_TX_XOFF;
                                }
                            }

                            goto ReceiveDoLineStatus;
                        }

                        // Check to see if we should note
                        // the receive character or special
                        // character event.

                        if (extension->IsrWaitMask) {

                            if (extension->IsrWaitMask &
                                SERIAL_EV_RXCHAR) {

                                extension->HistoryMask |= SERIAL_EV_RXCHAR;
                            }

                            if ((extension->IsrWaitMask &
                                 SERIAL_EV_RXFLAG) &&
                                (extension->SpecialChars.EventChar ==
                                 receivedChar)) {

                                extension->HistoryMask |= SERIAL_EV_RXFLAG;
                            }

                            if (extension->IrpMaskLocation &&
                                extension->HistoryMask) {

                                *extension->IrpMaskLocation =
                                 extension->HistoryMask;
                                extension->IrpMaskLocation = NULL;
                                extension->HistoryMask = 0;
                                reqContext = SerialGetRequestContext(extension->CurrentWaitRequest);
                                reqContext->Information = sizeof(ULONG);

                                SerialInsertQueueDpc(extension->CommWaitDpc);
                            }
                        }

                        SerialPutChar(extension, receivedChar);

                        // If we're doing line status and modem
                        // status insertion then we need to insert
                        // a zero following the character we just
                        // placed into the buffer to mark that this
                        // was reception of what we are using to
                        // escape.

                        if (extension->EscapeChar &&
                            (extension->EscapeChar ==
                             receivedChar)) {

                            SerialPutChar(extension, SERIAL_LSRMST_ESCAPE);
                        }

ReceiveDoLineStatus:
                        TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                    "SerialISR(o) [%lu]- miniUart RCV interrupt, do line status\r\n",
                                    ulIsrInnerLoopCnt);
                        
                        // This reads the interrupt ID register and detemines if bits are 0
                        // If either of the reserved bits are 1, we stop servicing interrupts
                        // Since this detection method is not guarenteed this is enabled via
                        // a registry entry "UartDetectRemoval" and intialized on DriverEntry.
                        // This is disabled by default and will only be enabled on Stratus systems
                        // that allow hot replacement of serial cards
                        
                        if(extension->UartRemovalDetect)
                        {
                           UCHAR detectRemoval;

                           detectRemoval = READ_INTERRUPT_ID_REG(extension, extension->Controller);

                           if(detectRemoval & SERIAL_IIR_MUST_BE_ZERO)
                           {
                               // break out of this loop and stop processing interrupts
                               
                               break;
                           }
                        }

                        // check bit 0 : 1=the receiver FIFO holds at least 1 symbol. 

                        if( !((tempLsr = SerialProcessLSR(extension)) & SERIAL_LSR_DR) ) 
                        { 

                            // No more characters, get out of the loop.

                            TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                        "SerialISR(o) [%lu] - miniUart RCV interrupt, no more chars\r\n",
                                        ulIsrInnerLoopCnt);
                            break;

                        };

                        if(receivedChar==0x00)
                            iReadCnt+=1;

                        if ((tempLsr & ~(SERIAL_LSR_THRE | SERIAL_LSR_TEMT |
                                         SERIAL_LSR_DR)) &&
                            extension->EscapeChar) {

                           // An error was indicated and inserted into the
                           // stream, get out of the loop.

                           TraceEvents(TRACE_LEVEL_WARNING, DBG_INTERRUPT, 
                                        "SerialISR(o) [%lu] - miniUart RCV interrupt, error LSR\r\n",
                                        ulIsrInnerLoopCnt);
                           break;
                        }

                        readFifoLvl--;

                    } while (readFifoLvl>0);

                    break;
                }

                case SERIAL_IIR_THR:
                {

                    TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                "SerialISR(o) [%lu]- miniUart TX interrupt, IIR=%02Xh\r\n",
                                ulIsrInnerLoopCnt,
                                interruptIdReg);

                    extension->HoldingEmpty = TRUE;

                    if (extension->WriteLength ||
                        extension->TransmitImmediate ||
                        extension->SendXoffChar ||
                        extension->SendXonChar) {

                        TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                    "SerialISR(o) [%lu] - miniUart TX interrupt, WrLen=%u XmtImmd=%d\r\n",
                                    ulIsrInnerLoopCnt,
                                    extension->WriteLength,
                                    extension->TransmitImmediate);
                        
                        // Even though all of the characters being
                        // sent haven't all been sent, this variable
                        // will be checked when the transmit queue is
                        // empty.  If it is still true and there is a
                        // wait on the transmit queue being empty then
                        // we know we finished transmitting all characters
                        // following the initiation of the wait since
                        // the code that initiates the wait will set
                        // this variable to false.
                        //
                        // One reason it could be false is that
                        // the writes were cancelled before they
                        // actually started, or that the writes
                        // failed due to timeouts.  This variable
                        // basically says a character was written
                        // by the isr at some point following the
                        // initiation of the wait.

                        extension->EmptiedTransmit = TRUE;

                        // If we have output flow control based on
                        // the modem status lines, then we have to do
                        // all the modem work before we output each
                        // character. (Otherwise we might miss a
                        // status line change.)

                        if (extension->HandFlow.ControlHandShake &
                            SERIAL_OUT_HANDSHAKEMASK) {

                            SerialHandleModemUpdate(extension, TRUE);
                        }

                        // We can only send the xon character if
                        // the only reason we are holding is because
                        // of the xoff.  (Hardware flow control or
                        // sending break preclude putting a new character
                        // on the wire.)

                        if (extension->SendXonChar &&
                            !(extension->TXHolding & ~SERIAL_TX_XOFF)) {

                            if ((extension->HandFlow.FlowReplace &
                                 SERIAL_RTS_MASK) ==
                                 SERIAL_TRANSMIT_TOGGLE) {

                                // We have to raise if we're sending
                                // this character.
                                
                                TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                            "SerialISR(o) [%lu] - miniUart TX interrupt, write Xon, SetRTS\r\n",
                                            ulIsrInnerLoopCnt);

                                SerialSetRTS(extension->WdfInterrupt, extension);

                                extension->PerfStats.TransmittedCount++;
                                extension->WmiPerfData.TransmittedCount++;

                                WRITE_TRANSMIT_HOLDING(extension, extension->Controller,
                                                        extension->SpecialChars.XonChar);

                                SerialInsertQueueDpc(extension->StartTimerLowerRTSDpc)
                                                    ?extension->CountOfTryingToLowerRTS++:0;


                            } else {
                                TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                            "SerialISR(o) [%lu] - miniUart TX interrupt, write Xon\r\n",
                                            ulIsrInnerLoopCnt);

                                extension->PerfStats.TransmittedCount++;
                                extension->WmiPerfData.TransmittedCount++;

                                WRITE_TRANSMIT_HOLDING(extension,
                                                        extension->Controller,
                                                        extension->SpecialChars.XonChar);
                            }

                            extension->SendXonChar = FALSE;
                            extension->HoldingEmpty = FALSE;

                            // If we send an xon, by definition we
                            // can't be holding by Xoff.

                            extension->TXHolding &= ~SERIAL_TX_XOFF;

                            // If we are sending an xon char then
                            // by definition we can't be "holding"
                            // up reception by Xoff.

                            extension->RXHolding &= ~SERIAL_RX_XOFF;

                        } else if (extension->SendXoffChar &&
                              !extension->TXHolding) {

                            if ((extension->HandFlow.FlowReplace &
                                 SERIAL_RTS_MASK) ==
                                 SERIAL_TRANSMIT_TOGGLE) {

                                // We have to raise if we're sending
                                // this character.

                                TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                            "SerialISR(o) [%lu] - miniUart TX interrupt, write Xoff,set RTS\r\n",
                                            ulIsrInnerLoopCnt);

                                SerialSetRTS(extension->WdfInterrupt, extension);

                                extension->PerfStats.TransmittedCount++;
                                extension->WmiPerfData.TransmittedCount++;

                                WRITE_TRANSMIT_HOLDING(extension,
                                                        extension->Controller,
                                                        extension->SpecialChars.XoffChar);

                                SerialInsertQueueDpc(extension->StartTimerLowerRTSDpc)
                                                    ?extension->CountOfTryingToLowerRTS++:0;

                            } else {
                                TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT,
                                            "SerialISR(o) [%lu] - miniUart TX interrupt, write Xoff\r\n",
                                            ulIsrInnerLoopCnt);

                                extension->PerfStats.TransmittedCount++;
                                extension->WmiPerfData.TransmittedCount++;

                                WRITE_TRANSMIT_HOLDING(extension,
                                                        extension->Controller,
                                                        extension->SpecialChars.XoffChar);

                            }

                            // We can't be sending an Xoff character
                            // if the transmission is already held
                            // up because of Xoff.  Therefore, if we
                            // are holding then we can't send the char.

                            // If the application has set xoff continue
                            // mode then we don't actually stop sending
                            // characters if we send an xoff to the other
                            // side.

                            if (!(extension->HandFlow.FlowReplace &
                                  SERIAL_XOFF_CONTINUE)) {

                                extension->TXHolding |= SERIAL_TX_XOFF;

                                if ((extension->HandFlow.FlowReplace &
                                     SERIAL_RTS_MASK) ==
                                     SERIAL_TRANSMIT_TOGGLE) {
                                    TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                                "SerialISR(o) [%lu] - miniUart TX interrupt, queue start timer"
                                                " lower Rts\r\n",
                                                ulIsrInnerLoopCnt);

                                    SerialInsertQueueDpc(extension->StartTimerLowerRTSDpc)
                                                        ?extension->CountOfTryingToLowerRTS++:0;
                                }
                            }

                            extension->SendXoffChar = FALSE;
                            extension->HoldingEmpty = FALSE;

                        // Even if transmission is being held
                        // up, we should still transmit an immediate
                        // character if all that is holding us
                        // up is xon/xoff

                        } else if (extension->TransmitImmediate &&
                            (!extension->TXHolding ||
                             (extension->TXHolding == SERIAL_TX_XOFF)
                            )) {

                            extension->TransmitImmediate = FALSE;

                            if ((extension->HandFlow.FlowReplace &
                                 SERIAL_RTS_MASK) ==
                                 SERIAL_TRANSMIT_TOGGLE) {

                                // We have to raise if we're sending
                                // this character.

                                TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                            "SerialISR(o) [%lu] - miniUart TX interrupt, write 1 byte, set RTS\r\n",
                                            ulIsrInnerLoopCnt);

                                SerialSetRTS(extension->WdfInterrupt, extension);

                                extension->PerfStats.TransmittedCount++;
                                extension->WmiPerfData.TransmittedCount++;

                                WRITE_TRANSMIT_HOLDING(extension,
                                                        extension->Controller,
                                                        extension->ImmediateChar);

                                SerialInsertQueueDpc(extension->StartTimerLowerRTSDpc)
                                                    ?extension->CountOfTryingToLowerRTS++:0;

                            } else {

                                TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                            "SerialISR(o) [%lu] - miniUart TX interrupt, write 1 byte, no RTS\r\n",
                                            ulIsrInnerLoopCnt);

                                extension->PerfStats.TransmittedCount++;
                                extension->WmiPerfData.TransmittedCount++;

                                WRITE_TRANSMIT_HOLDING(extension,
                                                        extension->Controller,
                                                        extension->ImmediateChar);

                            }

                            extension->HoldingEmpty = FALSE;

                            SerialInsertQueueDpc(extension->CompleteImmediateDpc);

                        } else if (!extension->TXHolding) {

                            ULONG amountToWrite;

                            if (extension->FifoPresent) {

                                amountToWrite = (extension->TxFifoAmount <
                                                 extension->WriteLength)?
                                                extension->TxFifoAmount:
                                                extension->WriteLength;
                            } else {

                                amountToWrite = 1;

                            }
                            if ((extension->HandFlow.FlowReplace &
                                 SERIAL_RTS_MASK) ==
                                 SERIAL_TRANSMIT_TOGGLE) {

                                // We have to raise if we're sending
                                // this character.
                                
                                TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                            "SerialISR(o) [%lu] - miniUart TX interrupt, writeXmit %u bytes, set RTS\r\n",
                                            ulIsrInnerLoopCnt,
                                            amountToWrite);
                                
                                SerialSetRTS(extension->WdfInterrupt, extension);

                                extension->PerfStats.TransmittedCount +=
                                    amountToWrite;
                                extension->WmiPerfData.TransmittedCount +=
                                    amountToWrite;

                                WRITE_TRANSMIT_FIFO_HOLDING(extension->Controller,
                                                            extension->WriteCurrentChar,
                                                            amountToWrite);

                                SerialInsertQueueDpc(extension->StartTimerLowerRTSDpc)
                                                    ?extension->CountOfTryingToLowerRTS++:0;

                            } else {

                                    TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT,
                                                "SerialISR(o) [%lu] - miniUart TX interrupt, writeXmit %u bytes \r\n",
                                                ulIsrInnerLoopCnt,
                                                amountToWrite);

                                    extension->PerfStats.TransmittedCount +=
                                        amountToWrite;
                                    extension->WmiPerfData.TransmittedCount +=
                                        amountToWrite;

                                    WRITE_TRANSMIT_FIFO_HOLDING(extension->Controller,
                                                                extension->WriteCurrentChar,
                                                                amountToWrite);
                            }

                            extension->HoldingEmpty = FALSE;
                            extension->WriteCurrentChar += amountToWrite;
                            extension->WriteLength -= amountToWrite;

                            if(0==extension->WriteLength) 
                            {
                                TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                                            "SerialISR [%lu] - write complete. Disable Tx intrpt\r\n",
                                            ulIsrInnerLoopCnt);
                                
                                // No More characters left.  This
                                // write is complete.  Take care
                                // when updating the information field,
                                // we could have an xoff counter masquerading
                                // as a write request.

                                // tx interrupt is on and tx fifo is not empty. Yet nothing to write. 
                                // Turn off tx interrupt now.
                                
                                uchIerTemp=READ_INTERRUPT_ENABLE(extension,extension->Controller);

                                WRITE_INTERRUPT_ENABLE(extension,extension->Controller, 
                                    (uchIerTemp & ~SERIAL_IER_THR));

                                reqContext = SerialGetRequestContext(extension->CurrentWriteRequest);

                                reqContext->Information =
                                    (reqContext->MajorFunction == IRP_MJ_WRITE)?
                                        (reqContext->Length): (1);

                                SerialInsertQueueDpc(extension->CompleteWriteDpc); 
                            }
                        }
                    }
                    else
                    {
                        // mini Uart extended status register bits 27-24 : Tx FIFO level

                        transmitFifoLvl=(SHORT)((READ_EXTRA_STATUS(extension->Controller) & 0x0F000000)>>24); 
                        TraceEvents(TRACE_LEVEL_WARNING, DBG_INTERRUPT, 
                                    "SerialISR [%lu] - stuck Tx interrupt. writelen=%Xh, Tx FIFO lvl=%u\r\n",
                                    ulIsrInnerLoopCnt,
                                    extension->WriteLength,
                                    transmitFifoLvl);

                        if (0 == extension->WriteLength) {

                            uchIerTemp=READ_INTERRUPT_ENABLE(extension,extension->Controller);
                            WRITE_INTERRUPT_ENABLE(extension,extension->Controller, 
                                (uchIerTemp & ~SERIAL_IER_THR));
                        }
                    }

                    break;
                }

                default: 
                TraceEvents(TRACE_LEVEL_WARNING, DBG_INTERRUPT, 
                            "SerialISR [%lu] - unsupported IIR content\r\n",
                            ulIsrInnerLoopCnt);
                break;

            };

            // determine if mini Uart is stuck inside ISR with receive interrupt and reading 0x00
            // (IIR will read C4 and Rx will read 0x00 byte), 
            // it is a known hardware condition and byte 0x00 is caused by Rx GPIO pin held low.
            // If this siatuation occurs, then allow ISR to return to avoid DPC watchdog timeout.
            // 
            // miniUart receive FIFO can hold max 8 bytes. If receiver got stuck, make note of it and quit the loop 

            if(iReadCnt>8) 
            {
                TraceEvents(TRACE_LEVEL_WARNING, DBG_INTERRUPT, 
                            "SerialISR [%lu] - receiver stuck with ReadInterrupt. Forced isr to quit loop.\r\n",
                            ulIsrInnerLoopCnt);
                break;
            }

            ulIsrInnerLoopCnt+=1;

        } while (!((interruptIdReg =
                    READ_INTERRUPT_ID_REG(extension, extension->Controller))
                    & SERIAL_IIR_NO_INTERRUPT_PENDING));

        TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                    "SerialISR() c=%lu out of loop. IIR=%02Xh\r\n",
                    ulIsrCallCount,
                    interruptIdReg);
    }

    TraceEvents(TRACE_LEVEL_ISROUTP, DBG_INTERRUPT, 
                "--SerialISR()=%lu c=%lu\r\n", servicedAnInterrupt,
                ulIsrCallCount);

    ulIsrCallCount+=1;
    return servicedAnInterrupt;
}

/*++

Routine Description:

    This routine, which only runs at device level, takes care of
    placing a character into the typeahead (receive) buffer.

Arguments:

    Extension - The serial device extension.

Return Value:

    None.

--*/
_Use_decl_annotations_
VOID
SerialPutChar(
     PSERIAL_DEVICE_EXTENSION Extension,
     UCHAR CharToPut
    )
{
    PREQUEST_CONTEXT reqContext = NULL;

    // If we have dsr sensitivity enabled then
    // we need to check the modem status register
    // to see if it has changed.

    if (Extension->HandFlow.ControlHandShake &
        SERIAL_DSR_SENSITIVITY) {

        SerialHandleModemUpdate(Extension, FALSE);

        if (Extension->RXHolding & SERIAL_RX_DSR) {

            // We simply act as if we haven't
            // seen the character if we have dsr
            // sensitivity and the dsr line is low.

            return;
        }
    }

    // If the xoff counter is non-zero then decrement it.
    // If the counter then goes to zero, complete that request.

    if (Extension->CountSinceXoff) {

        Extension->CountSinceXoff--;

        if (!Extension->CountSinceXoff) {
            reqContext = SerialGetRequestContext(Extension->CurrentXoffRequest);
            reqContext->Status = STATUS_SUCCESS;
            reqContext->Information = 0;

            SerialInsertQueueDpc(Extension->XoffCountCompleteDpc);
        }
    }

    // Check to see if we are copying into the
    // users buffer or into the interrupt buffer.
    //
    // If we are copying into the user buffer
    // then we know there is always room for one more.
    // (We know this because if there wasn't room
    // then that read would have completed and we
    // would be using the interrupt buffer.)
    //
    // If we are copying into the interrupt buffer
    // then we will need to check if we have enough
    // room.

    if (Extension->ReadBufferBase !=
        Extension->InterruptReadBuffer) {

        // Increment the following value so
        // that the interval timer (if one exists
        // for this read) can know that a character
        // has been read.

        Extension->ReadByIsr++;

        // We are in the user buffer.  Place the
        // character into the buffer.  See if the
        // read is complete.

        *Extension->CurrentCharSlot = CharToPut;

        if (Extension->CurrentCharSlot ==
            Extension->LastCharSlot) {

            // We've filled up the users buffer.
            // Switch back to the interrupt buffer
            // and send off a DPC to Complete the read.
            //
            // It is inherent that when we were using
            // a user buffer that the interrupt buffer
            // was empty.

            Extension->ReadBufferBase =
                Extension->InterruptReadBuffer;
            Extension->CurrentCharSlot =
                Extension->InterruptReadBuffer;
            Extension->FirstReadableChar =
                Extension->InterruptReadBuffer;
            Extension->LastCharSlot =
                Extension->InterruptReadBuffer +
                (Extension->BufferSize - 1);
            Extension->CharsInInterruptBuffer = 0;
            reqContext = SerialGetRequestContext(Extension->CurrentReadRequest);
            reqContext->Information = reqContext->Length;

            SerialInsertQueueDpc(Extension->CompleteReadDpc);

        } else {

            // Not done with the users read.

            Extension->CurrentCharSlot++;
        }

    } else {

        // We need to see if we reached our flow
        // control threshold.  If we have then
        // we turn on whatever flow control the
        // owner has specified.  If no flow
        // control was specified, well..., we keep
        // trying to receive characters and hope that
        // we have enough room.  Note that no matter
        // what flow control protocol we are using, it
        // will not prevent us from reading whatever
        // characters are available.

        if ((Extension->HandFlow.ControlHandShake
             & SERIAL_DTR_MASK) ==
            SERIAL_DTR_HANDSHAKE) {

            // If we are already doing a
            // dtr hold then we don't have
            // to do anything else.

            if (!(Extension->RXHolding &
                  SERIAL_RX_DTR)) {

                if ((Extension->BufferSize -
                     Extension->HandFlow.XoffLimit)
                    <= (Extension->CharsInInterruptBuffer+1)) {

                    Extension->RXHolding |= SERIAL_RX_DTR;

                    SerialClrDTR(Extension->WdfInterrupt, Extension);
                }
            }
        }

        if ((Extension->HandFlow.FlowReplace
             & SERIAL_RTS_MASK) ==
            SERIAL_RTS_HANDSHAKE) {

            // If we are already doing a
            // rts hold then we don't have
            // to do anything else.

            if (!(Extension->RXHolding &
                  SERIAL_RX_RTS)) {

                if ((Extension->BufferSize -
                     Extension->HandFlow.XoffLimit)
                    <= (Extension->CharsInInterruptBuffer+1)) {

                    Extension->RXHolding |= SERIAL_RX_RTS;

                    SerialClrRTS(Extension->WdfInterrupt, Extension);

                }
            }
        }

        if (Extension->HandFlow.FlowReplace &
            SERIAL_AUTO_RECEIVE) {

            // If we are already doing a
            // xoff hold then we don't have
            // to do anything else.

            if (!(Extension->RXHolding &
                  SERIAL_RX_XOFF)) {

                if ((Extension->BufferSize -
                     Extension->HandFlow.XoffLimit)
                    <= (Extension->CharsInInterruptBuffer+1)) {

                    Extension->RXHolding |= SERIAL_RX_XOFF;

                    // If necessary cause an
                    // off to be sent.

                    SerialProdXonXoff(Extension, FALSE);
                }
            }
        }

        if (Extension->CharsInInterruptBuffer <
            Extension->BufferSize) {

            *Extension->CurrentCharSlot = CharToPut;
            Extension->CharsInInterruptBuffer++;

            // If we've become 80% full on this character
            // and this is an interesting event, note it.

            if (Extension->CharsInInterruptBuffer ==
                Extension->BufferSizePt8) {

                if (Extension->IsrWaitMask &
                    SERIAL_EV_RX80FULL) {

                    Extension->HistoryMask |= SERIAL_EV_RX80FULL;

                    if (Extension->IrpMaskLocation) {

                        *Extension->IrpMaskLocation =
                         Extension->HistoryMask;
                        Extension->IrpMaskLocation = NULL;
                        Extension->HistoryMask = 0;

                        reqContext = SerialGetRequestContext(Extension->CurrentWaitRequest);
                        reqContext->Information =  sizeof(ULONG);

                        SerialInsertQueueDpc(Extension->CommWaitDpc);
                    }
                }
            }

            // Point to the next available space
            // for a received character.  Make sure
            // that we wrap around to the beginning
            // of the buffer if this last character
            // received was placed at the last slot
            // in the buffer.

            if (Extension->CurrentCharSlot ==
                Extension->LastCharSlot) {

                Extension->CurrentCharSlot =
                    Extension->InterruptReadBuffer;

            } else {

                Extension->CurrentCharSlot++;
            }

        } else {

            // We have a new character but no room for it.

            Extension->PerfStats.BufferOverrunErrorCount++;
            Extension->WmiPerfData.BufferOverrunErrorCount++;
            Extension->ErrorWord |= SERIAL_ERROR_QUEUEOVERRUN;

            if (Extension->HandFlow.FlowReplace &
                SERIAL_ERROR_CHAR) {

                // Place the error character into the last
                // valid place for a character.  Be careful!,
                // that place might not be the previous location!

                if (Extension->CurrentCharSlot ==
                    Extension->InterruptReadBuffer) {

                    *(Extension->InterruptReadBuffer+
                      (Extension->BufferSize-1)) =
                      Extension->SpecialChars.ErrorChar;

                } else {

                    *(Extension->CurrentCharSlot-1) =
                     Extension->SpecialChars.ErrorChar;
                }
            }

            // If the application has requested it, abort all reads
            // and writes on an error.

            if (Extension->HandFlow.ControlHandShake &
                SERIAL_ERROR_ABORT) {

                SerialInsertQueueDpc(Extension->CommErrorDpc);
            }
        }
    }
}

/*++

Routine Description:

    This routine, which only runs at device level, reads the
    ISR and totally processes everything that might have
    changed.

Arguments:

    Extension - The serial device extension.

Return Value:

    The value of the line status register.

--*/
_Use_decl_annotations_
UCHAR
SerialProcessLSR(
     PSERIAL_DEVICE_EXTENSION Extension
    )
{
    PREQUEST_CONTEXT reqContext = NULL;

    UCHAR lineStatus = READ_LINE_STATUS(Extension, Extension->Controller);

    Extension->HoldingEmpty = (lineStatus & SERIAL_LSR_THRE) ? TRUE : FALSE;

    // If the line status register is just the fact that
    // the trasmit registers are empty or a character is
    // received then we want to reread the interrupt
    // identification register so that we just pick up that.

    if (lineStatus & ~(SERIAL_LSR_THRE | SERIAL_LSR_TEMT
                       | SERIAL_LSR_DR)) {

        // We have some sort of data problem in the receive.
        // For any of these errors we may abort all current
        // reads and writes.
        //
        // If we are inserting the value of the line status
        // into the data stream then we should put the escape
        // character in now.

        if (Extension->EscapeChar) {

            SerialPutChar(Extension,
                            Extension->EscapeChar);

            SerialPutChar(Extension,
                        (UCHAR)((lineStatus & SERIAL_LSR_DR)?
                        (SERIAL_LSRMST_LSR_DATA):(SERIAL_LSRMST_LSR_NODATA)));

            SerialPutChar(Extension,
                            lineStatus);

            if (lineStatus & SERIAL_LSR_DR) {

                Extension->PerfStats.ReceivedCount++;
                Extension->WmiPerfData.ReceivedCount++;

                SerialPutChar(Extension,
                                READ_RECEIVE_BUFFER(Extension, Extension->Controller));
            }
        }

        if (lineStatus & SERIAL_LSR_OE) {

            Extension->PerfStats.SerialOverrunErrorCount++;
            Extension->WmiPerfData.SerialOverrunErrorCount++;
            Extension->ErrorWord |= SERIAL_ERROR_OVERRUN;

            if (Extension->HandFlow.FlowReplace &
                SERIAL_ERROR_CHAR) {

                SerialPutChar(Extension,
                                Extension->SpecialChars.ErrorChar);

                if (lineStatus & SERIAL_LSR_DR) {

                    Extension->PerfStats.ReceivedCount++;
                    Extension->WmiPerfData.ReceivedCount++;
                    READ_RECEIVE_BUFFER(Extension, Extension->Controller);
                }

            } else {

                if (lineStatus & SERIAL_LSR_DR) {

                    Extension->PerfStats.ReceivedCount++;
                    Extension->WmiPerfData.ReceivedCount++;

                    SerialPutChar(Extension,
                                    READ_RECEIVE_BUFFER(Extension, Extension->Controller));
                }
            }
        }

        if (lineStatus & SERIAL_LSR_BI) {

            Extension->ErrorWord |= SERIAL_ERROR_BREAK;

            if (Extension->HandFlow.FlowReplace &
                SERIAL_BREAK_CHAR) {

                SerialPutChar(Extension,
                                Extension->SpecialChars.BreakChar);
            }
        } else {

            // Framing errors only count if they
            // occur exclusive of a break being
            // received.

            if (lineStatus & SERIAL_LSR_PE) {

                Extension->PerfStats.ParityErrorCount++;
                Extension->WmiPerfData.ParityErrorCount++;
                Extension->ErrorWord |= SERIAL_ERROR_PARITY;

                if (Extension->HandFlow.FlowReplace &
                    SERIAL_ERROR_CHAR) {

                    SerialPutChar(Extension,
                                    Extension->SpecialChars.ErrorChar);

                    if (lineStatus & SERIAL_LSR_DR) {

                        Extension->PerfStats.ReceivedCount++;
                        Extension->WmiPerfData.ReceivedCount++;
                        READ_RECEIVE_BUFFER(Extension, Extension->Controller);
                    }
                }
            }

            if (lineStatus & SERIAL_LSR_FE) {

                Extension->PerfStats.FrameErrorCount++;
                Extension->WmiPerfData.FrameErrorCount++;
                Extension->ErrorWord |= SERIAL_ERROR_FRAMING;

                if (Extension->HandFlow.FlowReplace &
                    SERIAL_ERROR_CHAR) {

                    SerialPutChar(Extension,
                                    Extension->SpecialChars.ErrorChar);

                    if (lineStatus & SERIAL_LSR_DR) {

                        Extension->PerfStats.ReceivedCount++;
                        Extension->WmiPerfData.ReceivedCount++;
                        READ_RECEIVE_BUFFER(Extension, Extension->Controller);
                    }
                }
            }
        }

        // If the application has requested it,
        // abort all the reads and writes
        // on an error.

        if (Extension->HandFlow.ControlHandShake &
            SERIAL_ERROR_ABORT) {

            SerialInsertQueueDpc(Extension->CommErrorDpc);
        }

        // Check to see if we have a wait
        // pending on the comm error events.  If we
        // do then we schedule a dpc to satisfy
        // that wait.

        if (Extension->IsrWaitMask) {

            if ((Extension->IsrWaitMask & SERIAL_EV_ERR) &&
                (lineStatus & (SERIAL_LSR_OE |
                               SERIAL_LSR_PE |
                               SERIAL_LSR_FE))) {

                Extension->HistoryMask |= SERIAL_EV_ERR;
            }

            if ((Extension->IsrWaitMask & SERIAL_EV_BREAK) &&
                (lineStatus & SERIAL_LSR_BI)) {

                Extension->HistoryMask |= SERIAL_EV_BREAK;
            }

            if (Extension->IrpMaskLocation &&
                Extension->HistoryMask) {

                *Extension->IrpMaskLocation =
                 Extension->HistoryMask;
                Extension->IrpMaskLocation = NULL;
                Extension->HistoryMask = 0;
                reqContext = SerialGetRequestContext(Extension->CurrentWaitRequest);
                reqContext->Information =  sizeof(ULONG);

                SerialInsertQueueDpc(Extension->CommWaitDpc);
            }
        }

        if (lineStatus & SERIAL_LSR_THRE) {

            // There is a hardware bug in some versions
            // of 16550 chips.  If THRE interrupt
            // is pending, but a higher interrupt comes
            // in it will only return the higher and
            // *forget* about the THRE.
            //
            // A suitable workaround - whenever we
            // are *all* done reading line status
            // of the device we check to see if the
            // transmit holding register is empty.  If it is
            // AND we are currently transmitting data
            // enable the interrupts which should cause
            // an interrupt indication which we quiet
            // when we read the interrupt id register.

            if (Extension->WriteLength |
                Extension->TransmitImmediate) {

                DISABLE_ALL_INTERRUPTS(Extension, Extension->Controller);

                ENABLE_ALL_INTERRUPTS(Extension, Extension->Controller);
            }
        }
    }

    return lineStatus;
}


