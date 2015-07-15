/*++

    Raspberry Pi 2 (BCM2836) SPI Controller driver for SPB Framework

    Copyright (c) Microsoft Corporation

    All rights reserved.

    MIT License

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the ""Software""),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

Module Name: 

    controller.cpp

Abstract:

    This module contains the controller-specific functions
    for handling transfers and implementing interrupts.

Environment:

    kernel-mode only

Revision History:

--*/

#include "internal.h"
#include "controller.h"
#include "device.h"

#include "controller.tmh"

inline void
BCM_SPIFlushRxFifo(
    _In_  PPBC_DEVICE  pDevice
    )
/*++

    Routine Description:

        This routine flushes the Rx Fifo hardware and waits until the Tx Fifo is flushed.

    Arguments:

        pDevice - a pointer to the PBC device context

    Return Value:

        None.

--*/
{
    // must only be called on active transfer
    NT_ASSERT(READ_REGISTER_ULONG(&pDevice->pSPIRegisters->CS) & BCM_SPI_REG_CS_TA);
    // wait for Tx fifo empty
    int timeout = BCM_SPI_MAX_TIMEOUT_US;
    while (timeout > 0 &&
        (READ_REGISTER_ULONG(&pDevice->pSPIRegisters->CS) & BCM_SPI_REG_CS_DONE)==0)
    {   // do not flood the I/O bus
        KeStallExecutionProcessor(1);
        timeout--;
        // we should never timeout
        NT_ASSERT(timeout > 0);
    }
    // clear the Rx fifo
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY | BCM_SPI_REG_CS_CLEARRX);
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);
}

_Use_decl_annotations_
VOID
ControllerInitialize(
    PPBC_DEVICE  pDevice
    )
/*++
 
  Routine Description:

    This routine initializes the controller hardware.

  Arguments:

    pDevice - a pointer to the PBC device context

  Return Value:

    None.

--*/
{
    FuncEntry(TRACE_FLAG_PBCLOADING);
                        
    NT_ASSERT(pDevice != NULL);
    NT_ASSERT(pDevice->pSPIRegisters != NULL);

    pDevice->SPI_CS_COPY = BCM_SPI_REG_CS_POLL_DEFAULT;
    pDevice->CurrentConnectionSpeed = BCM_SPI_REG_CLK_DEFAULT;
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CLK, 
        BCM_SPI_REG_CLK_CDIV_SET(BCM_SPISetClkDivider(BCM_SPI_REG_CLK_DEFAULT)));

    FuncExit(TRACE_FLAG_PBCLOADING);
}

_Use_decl_annotations_
VOID
ControllerUninitialize(
    PPBC_DEVICE  pDevice
    )
/*++
 
  Routine Description:

    This routine uninitializes the controller hardware.

  Arguments:

    pDevice - a pointer to the PBC device context

  Return Value:

    None.

--*/
{
    FuncEntry(TRACE_FLAG_PBCLOADING);

    NT_ASSERT(pDevice != NULL);

    ControllerDisableInterrupts(pDevice);

    // make sure pending transactions are stopped
    pDevice->SPI_CS_COPY &= ~BCM_SPI_REG_CS_TA;
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);

    FuncExit(TRACE_FLAG_PBCLOADING);
}

_Use_decl_annotations_
VOID
ControllerConfigureForTransfer(
    PPBC_DEVICE   pDevice,
    PPBC_REQUEST  pRequest
    )
/*++
 
  Routine Description:

    This routine configures and starts the controller
    for a transfer.

  Arguments:

    pDevice - a pointer to the PBC device context
    pRequest - a pointer to the PBC request context

  Return Value:

    None. The request is completed asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NT_ASSERT(pDevice  != NULL);
    NT_ASSERT(pRequest != NULL);
    
    //
    // Initialize request context for transfer.
    //

    pRequest->Status = STATUS_SUCCESS;

    //
    // Configure hardware for transfer.
    //
    if (pRequest->SequencePosition == SpbRequestSequencePositionSingle ||
        pRequest->SequencePosition == SpbRequestSequencePositionFirst)
    {
        PPBC_TARGET_SETTINGS pSettings = &pDevice->pCurrentTarget->Settings;
        if (pDevice->CurrentConnectionSpeed != pSettings->ConnectionSpeed)
        {
            pDevice->CurrentConnectionSpeed = pSettings->ConnectionSpeed;

            // set clock
            WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CLK,
                BCM_SPI_REG_CLK_CDIV_SET(BCM_SPISetClkDivider(pDevice->CurrentConnectionSpeed)));
        }

        // set chip select, CPHA and CPOL
        pDevice->SPI_CS_COPY &= ~(BCM_SPI_REG_CS_CS | BCM_SPI_REG_CS_CPHA | BCM_SPI_REG_CS_CPOL);
        pDevice->SPI_CS_COPY |= BCM_SPI_REG_CS_CS_SET(pSettings->DeviceSelection);

        // CPOL
        if (pSettings->Polarity)
        {
            pDevice->SPI_CS_COPY |= BCM_SPI_REG_CS_CPOL;
        }

        // CPHA
        if (pSettings->Phase)
        {
            pDevice->SPI_CS_COPY |= BCM_SPI_REG_CS_CPHA;
        }

        // WireMode, only 4 wire supported yet
        NT_ASSERT((pSettings->TypeSpecificFlags & SPI_WIREMODE_BIT)==0);

        // DevicePolarity
        ULONG csPolarityBit = BCM_SPI_REG_CS_CSPOL0 << pSettings->DeviceSelection;
        if (pSettings->TypeSpecificFlags & SPI_DEVICEPOLARITY_BIT)
        {   // active high
            pDevice->SPI_CS_COPY |= csPolarityBit | BCM_SPI_REG_CS_CSPOL;
        }
        else
        {   // active low
            pDevice->SPI_CS_COPY &= ~(csPolarityBit | BCM_SPI_REG_CS_CSPOL);
        }

        // reset Rx Fifo
        WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY | BCM_SPI_REG_CS_CLEARRX);
        WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);

        // start transfer
        NT_ASSERT((pDevice->SPI_CS_COPY & BCM_SPI_REG_CS_TA)==0);
        pDevice->SPI_CS_COPY |= BCM_SPI_REG_CS_TA;
        WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);
    }
    else
    {
        NT_ASSERT((pDevice->SPI_CS_COPY & BCM_SPI_REG_CS_TA) == BCM_SPI_REG_CS_TA);
        NT_ASSERT(READ_REGISTER_ULONG(&pDevice->pSPIRegisters->CS) & BCM_SPI_REG_CS_TA);
    }

    NT_ASSERT(pRequest->Direction < SpbTransferDirectionMax);

    ControllerTransferData(pDevice, pRequest);

    //
    // Synchronize access to device context with ISR.
    //

    WdfInterruptAcquireLock(pDevice->InterruptObject);

    //
    // Set interrupt mask and clear current status.
    //

    pRequest->DataReadyFlag = BCM_SPI_REG_CS_DONE;
    PbcDeviceSetInterruptMask(pDevice, BCM_SPI_REG_CS_DONE);

    pDevice->InterruptStatus = 0;

    Trace(
        TRACE_LEVEL_VERBOSE,
        TRACE_FLAG_TRANSFER,
        "Controller configured for %s of %Iu bytes to device 0x%lx (SPBREQUEST %p, WDFDEVICE %p)",
        pRequest->Direction == SpbTransferDirectionFromDevice ? "read" : 
            pRequest->Direction == SpbTransferDirectionToDevice ? "write" : "fullduplex",
        pRequest->Length,
        pDevice->pCurrentTarget->Settings.DeviceSelection,
        pRequest->SpbRequest,
        pDevice->FxDevice);

    ControllerEnableInterrupts(
        pDevice,
        PbcDeviceGetInterruptMask(pDevice));

    WdfInterruptReleaseLock(pDevice->InterruptObject);

    FuncExit(TRACE_FLAG_TRANSFER);
}

_Use_decl_annotations_
VOID
ControllerProcessInterrupts(
    PPBC_DEVICE   pDevice,
    PPBC_REQUEST  pRequest,
    ULONG         InterruptStatus
    )
/*++
 
  Routine Description:

    This routine processes a hardware interrupt. Activities
    include checking for errors and transferring data.

  Arguments:

    pDevice - a pointer to the PBC device context
    pRequest - a pointer to the PBC request context
    InterruptStatus - saved interrupt status bits from the ISR.
        These have already been acknowledged and disabled

  Return Value:

    None. The request is completed asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NTSTATUS status;

    NT_ASSERT(pDevice  != NULL);
    NT_ASSERT(pRequest != NULL);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Ready to process interrupts with status 0x%lx for WDFDEVICE %p",
        InterruptStatus,
        pDevice->FxDevice);
    
    //
    // Check if controller is ready to transfer more data.
    // Check if transfer is complete.
    //

    if (TestAnyBits(InterruptStatus, pRequest->DataReadyFlag))
    {
        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_TRANSFER,
            "Transfer complete for device 0x%lx with %Iu bytes remaining",
            pDevice->pCurrentTarget->Settings.DeviceSelection,
            PbcRequestGetInfoRemaining(pRequest));
        
        //
        // If transfer complete interrupt occured and there
        // are still bytes remaining, transfer data. 
        //

        if (PbcRequestGetInfoRemaining(pRequest) > 0)
        {
            status = ControllerTransferData(pDevice, pRequest);

            if (!NT_SUCCESS(status))
            {
                pRequest->Status = status;

                Trace(
                    TRACE_LEVEL_ERROR, 
                    TRACE_FLAG_TRANSFER,
                    "Unexpected error while transferring data for device 0x%lx, "
                    "completing transfer and resetting controller "
                    "(WDFDEVICE %p) - %!STATUS!",
                    pDevice->pCurrentTarget->Settings.DeviceSelection,
                    pDevice->FxDevice,
                    pRequest->Status);

                //
                // Complete the transfer and stop processing
                // interrupts.
                //
                pRequest->DataReadyFlag = 0;
                ControllerCompleteTransfer(pDevice, pRequest, TRUE);
                goto exit;
            }
        }

        //
        // Complete the transfer.
        //
        if (PbcRequestGetInfoRemaining(pRequest) == 0)
        {
            pRequest->DataReadyFlag = 0;
            ControllerCompleteTransfer(pDevice, pRequest, FALSE);
        }
    }

exit:

    FuncExit(TRACE_FLAG_TRANSFER);
}

_Use_decl_annotations_
NTSTATUS
ControllerTransferData(
    PPBC_DEVICE   pDevice,
    PPBC_REQUEST  pRequest
    )
/*++
 
  Routine Description:

    This routine transfers data to or from the device.

  Arguments:

    pDevice - a pointer to the PBC device context
    pRequest - a pointer to the PBC request context

  Return Value:

    None.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    size_t bytesToTransfer = min(pRequest->Length - pRequest->Information, BCM_SPI_MAX_BYTES_PER_TRANSFER);
    size_t bytesTransferred = 0;
    NTSTATUS status = STATUS_SUCCESS;
    UCHAR nextByte;

    //
    // Write
    //

    if (pRequest->Direction == SpbTransferDirectionToDevice)
    {
        Trace(
            TRACE_LEVEL_INFORMATION, 
            TRACE_FLAG_TRANSFER,
            "Ready to write %Iu byte(s) for device 0x%lx", 
            bytesToTransfer,
            pDevice->pCurrentTarget->Settings.DeviceSelection);

        // clear the read fifo first
        BCM_SPIFlushRxFifo(pDevice);

        while (bytesTransferred < bytesToTransfer)
        {
            status = PbcRequestGetByte(pRequest, pRequest->Information + bytesTransferred, &nextByte);
            if (NT_SUCCESS(status))
            {
                WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO, (ULONG)nextByte);
                bytesTransferred++;
            }
            else
            {
                break;
            }
        }

        pRequest->InformationWritten += bytesTransferred;
        pRequest->Information += bytesTransferred;

    }

    //
    // Read
    //

    else if (pRequest->Direction == SpbTransferDirectionFromDevice)
    {

        Trace(
            TRACE_LEVEL_INFORMATION, 
            TRACE_FLAG_TRANSFER,
            "Ready to read %Iu byte(s) for device 0x%lx", 
            bytesToTransfer,
            pDevice->pCurrentTarget->Settings.DeviceSelection);

        // make sure we don't read data from the Rx fifo from a previous write transaction
        if (pRequest->InformationWritten == 0)
        {
            BCM_SPIFlushRxFifo(pDevice);
        }

        // read pending Rx data
        while (bytesTransferred < bytesToTransfer)
        {
            // more data in Rx Fifo?
            if (READ_REGISTER_ULONG(&pDevice->pSPIRegisters->CS) & BCM_SPI_REG_CS_RXD)
            {
                nextByte = (UCHAR)READ_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO);
                status = PbcRequestSetByte(pRequest->pMdlChain, pRequest->Information + bytesTransferred, pRequest->Length, nextByte);
                if (NT_SUCCESS(status))
                {
                    bytesTransferred++;
                }
                else
                {
                    NT_ASSERT(!"the buffer is too small or we read too many bytes");
                    break;
                }
            }
            else
            {
                // catch this logic error, it means the read request is stuck if we have nothing more to transmit
                NT_ASSERT(pRequest->Length > pRequest->InformationWritten);
                break;
            }
        }

        pRequest->Information += bytesTransferred;

        // now the Rx buffer is empty
        bytesTransferred = 0;
        size_t bytesToWrite = min(pRequest->Length - pRequest->InformationWritten, BCM_SPI_MAX_BYTES_PER_TRANSFER);

        // dummy data for Tx
        while (bytesTransferred < bytesToWrite)
        {
            // write to the fifo to start the read transactions
            WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO, 0);
            bytesTransferred++;
        }
        pRequest->InformationWritten += bytesTransferred;

    }
    //
    // full duplex
    //
    else
    {
        size_t readBytesToTransfer = min(pRequest->FullDuplexLength - pRequest->Information, BCM_SPI_MAX_BYTES_PER_TRANSFER);

        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_TRANSFER,
            "Ready to read/write %Iu byte(s) for device 0x%lx",
            readBytesToTransfer,
            pDevice->pCurrentTarget->Settings.DeviceSelection);

        // make sure we don't read data from the Rx fifo from a previous transaction
        if (pRequest->InformationWritten == 0)
        {
            BCM_SPIFlushRxFifo(pDevice);
        }

        // read pending Rx data
        while (bytesTransferred < readBytesToTransfer)
        {
            // more data in Rx Fifo?
            if (READ_REGISTER_ULONG(&pDevice->pSPIRegisters->CS) & BCM_SPI_REG_CS_RXD)
            {
                nextByte = (UCHAR)READ_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO);
                PbcRequestSetByte(pRequest->pFullDuplexReadMdlChain, pRequest->Information + bytesTransferred, pRequest->FullDuplexReadLength, nextByte);
                bytesTransferred++;
            }
            else
            {
                // catch this logic error, it means the read request is stuck if we have nothing more to transmit
                NT_ASSERT(pRequest->FullDuplexLength > pRequest->InformationWritten);
                break;
            }
        }

        pRequest->Information += bytesTransferred;

        // now the Rx buffer is empty
        bytesTransferred = 0;
        size_t bytesToWrite = min(pRequest->FullDuplexLength - pRequest->InformationWritten, BCM_SPI_MAX_BYTES_PER_TRANSFER);

        // write Tx data to fifo
        while (bytesTransferred < bytesToWrite)
        {
            // if the write buffer is exceeded just write 0
            status = PbcRequestGetByte(pRequest, pRequest->InformationWritten + bytesTransferred, &nextByte);

            if (NT_SUCCESS(status))
            {
                WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO, nextByte);
            }
            else
            {
                WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO, 0);
            }

            bytesTransferred++;
        }
        status = STATUS_SUCCESS;
        pRequest->InformationWritten += bytesTransferred;
    }

    FuncExit(TRACE_FLAG_TRANSFER);
    
    return status;
}

_Use_decl_annotations_
VOID
ControllerCompleteTransfer(
    PPBC_DEVICE   pDevice,
    PPBC_REQUEST  pRequest,
    BOOLEAN       AbortSequence
    )
/*++
 
  Routine Description:

    This routine completes a data transfer. Unless there are
    more transfers remaining in the sequence, the request is
    completed.

  Arguments:

    pDevice - a pointer to the PBC device context
    pRequest - a pointer to the PBC request context
    AbortSequence - specifies whether the driver should abort the
        ongoing sequence or begin the next transfer

  Return Value:

    None. The request is completed asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NT_ASSERT(pDevice  != NULL);
    NT_ASSERT(pRequest != NULL);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Transfer (index %lu) %s with %Iu bytes for device 0x%lx (SPBREQUEST %p)",
        pRequest->TransferIndex,
        NT_SUCCESS(pRequest->Status) ? "complete" : "error",
        pRequest->Information,
        pDevice->pCurrentTarget->Settings.DeviceSelection,
        pRequest->SpbRequest);

    //
    // Update request context with information from this transfer.
    //

    pRequest->TotalInformation += pRequest->Information;
    pRequest->Information = 0;
    pRequest->InformationWritten = 0;

    //
    // Check if there are more transfers
    // in the sequence.
    //

    if (!AbortSequence)
    {
        pRequest->TransferIndex++;

        if (pRequest->TransferIndex < pRequest->TransferCount)
        {
            //
            // Configure the request for the next transfer.
            //

            pRequest->Status = PbcRequestConfigureForIndex(
                pRequest, 
                pRequest->TransferIndex);

            if (NT_SUCCESS(pRequest->Status))
            {
                //
                // Configure controller and kick-off read.
                // Request will be completed asynchronously.
                //

                PbcRequestDoTransfer(pDevice,pRequest);
                goto exit;
            }
        }
    }

    //
    // If not already cancelled, unmark request cancellable.
    //

    if (pRequest->Status != STATUS_CANCELLED)
    {
        NTSTATUS cancelStatus;
        cancelStatus = WdfRequestUnmarkCancelable(pRequest->SpbRequest);

        if (!NT_SUCCESS(cancelStatus))
        {
            //
            // WdfRequestUnmarkCancelable should only fail if the request
            // has already been or is about to be cancelled. If it does fail 
            // the request must NOT be completed - the cancel callback will do
            // this.
            //

            NT_ASSERTMSG("WdfRequestUnmarkCancelable should only fail if the request has already been or is about to be cancelled",
                cancelStatus == STATUS_CANCELLED);

            Trace(
                TRACE_LEVEL_INFORMATION,
                TRACE_FLAG_TRANSFER,
                "Failed to unmark SPBREQUEST %p as cancelable - %!STATUS!",
                pRequest->SpbRequest,
                cancelStatus);

            goto exit;
        }
    }

    //
    // Done or error occurred. Set interrupt mask to 0.
    // Doing this keeps the DPC from re-enabling interrupts.
    //

    PbcDeviceSetInterruptMask(pDevice, 0);
    ControllerDisableInterrupts(pDevice);

    //
    // Clear the target's current request. This will prevent
    // the request context from being accessed once the request
    // is completed (and the context is invalid).
    //

    pDevice->pCurrentTarget->pCurrentRequest = NULL;

    //
    // end the current transfer if this was a single sequence or the last
    //

    // clear the read fifo first
    BCM_SPIFlushRxFifo(pDevice);

    // deassert cs only if not between lock / unlock pair
    if (!pDevice->Locked)
    {
        if (pRequest->SequencePosition == SpbRequestSequencePositionSingle ||
            pRequest->SequencePosition == SpbRequestSequencePositionLast)
        {
            NT_ASSERT(pDevice->SPI_CS_COPY & BCM_SPI_REG_CS_TA);
            // stop transfer
            pDevice->SPI_CS_COPY &= ~BCM_SPI_REG_CS_TA;
            WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);
        }

        //
        // Clear the controller's current target if any of
        //   1. request is type sequence
        //   2. request position is single 
        //      (did not come between lock/unlock)
        // Otherwise wait until unlock.
        //

        if ((pRequest->Type == SpbRequestTypeSequence) ||
            (pRequest->SequencePosition == SpbRequestSequencePositionSingle))
        {
            pDevice->pCurrentTarget = NULL;
        }
    }

    // correct the result for a full duplex transfer
    if (pRequest->SequencePosition == SpbRequestSequencePositionSingle &&
        pRequest->Direction == SpbTransferDirectionNone)
    {
        NT_ASSERT(pRequest->FullDuplexLength != 0);
        // the size of a full duplex request is the length of the write + read buffer
        pRequest->TotalInformation = pRequest->Length + pRequest->FullDuplexReadLength;
    }

    //
    // Mark the IO complete. Request not
    // completed here.
    //

    pRequest->bIoComplete = TRUE;

exit:

    FuncExit(TRACE_FLAG_TRANSFER);
}

_Use_decl_annotations_
VOID
ControllerUnlockTransfer(
    PPBC_DEVICE   pDevice
)
/*++

  Routine Description:

    This routine completes a locked data transfer.

  Arguments:

    pDevice - a pointer to the PBC device context

  Return Value:

    None. The request is completed asynchronously.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NT_ASSERT(pDevice != NULL);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Unlock for device 0x%lx ",
        pDevice->pCurrentTarget->Settings.DeviceSelection);

    //
    // end the current transfer
    //

    // for an empty lock/unlock pair without transaction the TA bit may not even be set
    if (pDevice->SPI_CS_COPY & BCM_SPI_REG_CS_TA)
    {
        // clear the read fifo first
        BCM_SPIFlushRxFifo(pDevice);
        // stop transfer
        pDevice->SPI_CS_COPY &= ~BCM_SPI_REG_CS_TA;
        WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);
    }

    FuncExit(TRACE_FLAG_TRANSFER);
}

_Use_decl_annotations_
VOID
ControllerEnableInterrupts(
    PPBC_DEVICE   pDevice,
    ULONG         InterruptMask
    )
/*++
 
  Routine Description:

    This routine enables the hardware interrupts for the
    specificed mask.

  Arguments:

    pDevice - a pointer to the PBC device context
    InterruptMask - interrupt bits to enable

  Return Value:

    None.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NT_ASSERT(pDevice != NULL);

    Trace(
        TRACE_LEVEL_VERBOSE,
        TRACE_FLAG_TRANSFER,
        "Enable interrupts with mask 0x%lx (WDFDEVICE %p)",
        InterruptMask,
        pDevice->FxDevice);

    // The SPI interface exposes irq enable and status in different bits...
    if (InterruptMask & BCM_SPI_REG_CS_DONE)
    {
        InterruptMask |= BCM_SPI_REG_CS_INTD;
    }
    if (InterruptMask & BCM_SPI_REG_CS_RXR)
    {
        InterruptMask |= BCM_SPI_REG_CS_INTR;
    }

    pDevice->SPI_CS_COPY |= InterruptMask;
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);

    FuncExit(TRACE_FLAG_TRANSFER);
}

_Use_decl_annotations_
VOID
ControllerDisableInterrupts(
    PPBC_DEVICE   pDevice
    )
/*++
 
  Routine Description:

    This routine disables all controller interrupts.

  Arguments:

    pDevice - a pointer to the PBC device context

  Return Value:

    None.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NT_ASSERT(pDevice != NULL);

    pDevice->SPI_CS_COPY &= (ULONG)~(
        BCM_SPI_REG_CS_INTR | BCM_SPI_REG_CS_INTD | 
        BCM_SPI_REG_CS_RXR | BCM_SPI_REG_CS_DONE
        );
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);

    FuncExit(TRACE_FLAG_TRANSFER);
}

_Use_decl_annotations_
ULONG
ControllerGetInterruptStatus(
    PPBC_DEVICE   pDevice,
    ULONG         InterruptMask
    )
/*++
 
  Routine Description:

    This routine gets the interrupt status of the
    specificed interrupt bits.

  Arguments:

    pDevice - a pointer to the PBC device context
    InterruptMask - interrupt bits to check

  Return Value:

    A bitmap indicating which interrupts are set.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    ULONG interruptStatus = 0;

    NT_ASSERT(pDevice != NULL);

    // The SPI interface exposes irq enable and status in different bits...
    interruptStatus = READ_REGISTER_ULONG(&pDevice->pSPIRegisters->CS);
    if (!(interruptStatus & BCM_SPI_REG_CS_INTR))
    {
        interruptStatus &= ~BCM_SPI_REG_CS_RXR;
    }
    if (!(interruptStatus & BCM_SPI_REG_CS_INTD))
    {
        interruptStatus &= ~BCM_SPI_REG_CS_DONE;
    }
    interruptStatus &= InterruptMask;

    FuncExit(TRACE_FLAG_TRANSFER);

    return interruptStatus;
}

_Use_decl_annotations_
VOID
ControllerAcknowledgeInterrupts(
    PPBC_DEVICE   pDevice,
    ULONG         InterruptMask
    )
/*++
 
  Routine Description:

    This routine acknowledges the
    specificed interrupt bits.

  Arguments:

    pDevice - a pointer to the PBC device context
    InterruptMask - interrupt bits to acknowledge

  Return Value:

    None.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NT_ASSERT(pDevice != NULL);

    // intentional no op

    UNREFERENCED_PARAMETER(pDevice);
    UNREFERENCED_PARAMETER(InterruptMask);

    FuncExit(TRACE_FLAG_TRANSFER);
}
