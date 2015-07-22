/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

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

const PBC_TRANSFER_SETTINGS g_TransferSettings[] =
{
    // Bus condition        IsStart  IsEnd
    { BusConditionDontCare, FALSE,  FALSE }, // SpbRequestTypeInvalid
    { BusConditionFree,     TRUE,   TRUE },  // SpbRequestTypeSingle
    { BusConditionFree,     TRUE,   FALSE }, // SpbRequestTypeFirst
    { BusConditionBusy,     FALSE,  FALSE }, // SpbRequestTypeContinue
    { BusConditionBusy,     FALSE,  TRUE }   // SpbRequestTypeLast
};

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
    NT_ASSERT(pDevice->pRegisters != NULL);

    pDevice->I2C_CONTROL_COPY = BCM_I2C_REG_CONTROL_DEFAULT;
    pDevice->CurrentConnectionSpeed = BCM_I2C_CLOCK_RATE_STANDARD;
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->Control, pDevice->I2C_CONTROL_COPY);
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->Status, BCM_I2C_REG_STATUS_DONE | BCM_I2C_REG_STATUS_ERR | BCM_I2C_REG_STATUS_CLKT);
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->ClockDivider, BCMI2CSetClkDivider(pDevice->CurrentConnectionSpeed));
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->ClockStretchTimeout, BCM_I2C_REG_CLKT_TOUT);
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->DataDelay, BCM_I2C_REG_DEL_DEFAULT);

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

    pDevice->I2C_CONTROL_COPY = 0;
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->Control, pDevice->I2C_CONTROL_COPY | BCM_I2C_REG_CONTROL_CLEAR);

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

    NT_ASSERT(pDevice != NULL);
    NT_ASSERT(pRequest != NULL);

    ULONG InterruptMask = 0;

    //
    // Initialize request context for transfer.
    //

    pRequest->Settings = g_TransferSettings[pRequest->SequencePosition];
    pRequest->Status = STATUS_SUCCESS;

    //
    // Configure hardware for transfer.
    //
    if (pRequest->Direction == SpbTransferDirectionToDevice)
    {
        // set write transfer
        pDevice->I2C_CONTROL_COPY &= ~BCM_I2C_REG_CONTROL_READ;
        // process only TXW interrupts
        pRequest->DataReadyFlag = BCM_I2C_REG_STATUS_TXW;
        // enable done and TXW irq
        InterruptMask =
            BCM_I2C_REG_STATUS_TXW |
            BCM_I2C_REG_STATUS_DONE;
    }
    else if (pRequest->Direction == SpbTransferDirectionFromDevice)
    {
        // set read transfer
        pDevice->I2C_CONTROL_COPY |= BCM_I2C_REG_CONTROL_READ;
        // process only RXR interrupts
        pRequest->DataReadyFlag = BCM_I2C_REG_STATUS_RXR;
        // enable done and RXR irq
        InterruptMask =
            BCM_I2C_REG_STATUS_RXR |
            BCM_I2C_REG_STATUS_DONE;

    }
    else
    {
        NT_ASSERT(!"unexpected SpbTransferDirection");
    }

    if (pRequest->Settings.IsStart)
    {
        // controller must not be in a transfer at this point
        NT_ASSERT((READ_REGISTER_ULONG(&pDevice->pRegisters->Status) & BCM_I2C_REG_STATUS_TA) == 0);

        // reset status bits to a well known state, clear error and done flags
        WRITE_REGISTER_ULONG(&pDevice->pRegisters->Status,
            BCM_I2C_REG_STATUS_DONE |
            BCM_I2C_REG_STATUS_ERR |
            BCM_I2C_REG_STATUS_CLKT);

        // set I2C clock speed if necessary
        PPBC_TARGET_SETTINGS pSettings = &pDevice->pCurrentTarget->Settings;
        NT_ASSERT(pSettings != NULL);
        if (pDevice->CurrentConnectionSpeed != pSettings->ConnectionSpeed)
        {
            pDevice->CurrentConnectionSpeed = pSettings->ConnectionSpeed;
            NT_ASSERT(BCMI2CSetClkDivider(pDevice->CurrentConnectionSpeed) <= BCM_I2C_REG_DIV_CDIV);
            WRITE_REGISTER_ULONG(&pDevice->pRegisters->ClockDivider, BCMI2CSetClkDivider(pDevice->CurrentConnectionSpeed));
        }

        // set I2C device address
        NT_ASSERT(pSettings->Address <= BCM_I2C_REG_ADDRESS_MASK);
        WRITE_REGISTER_ULONG(&pDevice->pRegisters->SlaveAddress, pSettings->Address);
    }

    if (!pRequest->RepeatedStart)
    {
        // set transfer length
        NT_ASSERT(pRequest->Length <= BCM_I2C_MAX_TRANSFER_LENGTH);
        WRITE_REGISTER_ULONG(&pDevice->pRegisters->DataLength, (ULONG)pRequest->Length);

        // start transfer and clear fifo
        WRITE_REGISTER_ULONG(&pDevice->pRegisters->Control,
            pDevice->I2C_CONTROL_COPY | BCM_I2C_REG_CONTROL_CLEAR | BCM_I2C_REG_CONTROL_ST);
    }
    else if (pRequest->Settings.IsEnd)
    {
        // clear repeated start on the last transfer, otherwise the DONE flag is never acknoledged
        pRequest->RepeatedStart = FALSE;
    }

    // special handling for repeated start condition 
    // (only support writeread case)
    if (!pRequest->Settings.IsEnd && 
        pRequest->Direction == SpbTransferDirectionToDevice &&
        pRequest->TransferIndex == 0 &&
        pRequest->TransferCount == 2)
    {
        SPB_TRANSFER_DESCRIPTOR descriptor;
        PMDL pMdl;
        SPB_TRANSFER_DESCRIPTOR_INIT(&descriptor);

        // peek into the next transfer parameters
        SpbRequestGetTransferParameters(
            pRequest->SpbRequest,
            pRequest->TransferIndex + 1,
            &descriptor,
            &pMdl);

        // make sure the second transfer is a read
        if (descriptor.Direction == SpbTransferDirectionFromDevice)
        {
            ULONG timeout = BCM_TA_BIT_TIMEOUT;
            // this loop is time critical, in order to force a repeated start the next transfer
            // must be started before the current transfer stops. Loop until TA=1.
            while (--timeout > 0 &&
                (READ_REGISTER_ULONG(&pDevice->pRegisters->Status) & BCM_I2C_REG_STATUS_TA) == 0)
            {
                KeStallExecutionProcessor(1);
            }

            // if we missed the TA bit, fall back
            if (timeout != 0)
            {
                // set length for NEXT transfer
                NT_ASSERT(descriptor.TransferLength <= BCM_I2C_MAX_TRANSFER_LENGTH);
                WRITE_REGISTER_ULONG(&pDevice->pRegisters->DataLength, (ULONG)descriptor.TransferLength);

                pDevice->I2C_CONTROL_COPY |= BCM_I2C_REG_CONTROL_READ;

                // enable RXR for read transfers which exceed the fifo size
                // we need it to signal the previous write transfer 
                // completion in certain cases
                pRequest->DataReadyFlag |= BCM_I2C_REG_STATUS_RXR;
                InterruptMask |= BCM_I2C_REG_STATUS_RXR;

                // latch the read transfer to force the repeated start
                WRITE_REGISTER_ULONG(&pDevice->pRegisters->Control,
                    pDevice->I2C_CONTROL_COPY | BCM_I2C_REG_CONTROL_ST);

                // remember that the controller is setup for a repeated start
                pRequest->RepeatedStart = TRUE;
            }
        }
    }

    if (pRequest->Direction == SpbTransferDirectionToDevice)
    {
        // fill fifo
        ControllerTransferData(pDevice, pRequest);

        // for small transfers which fit completely in the FIFO 
        // turn off the TXW data irq and just wait for DONE
        if (pRequest->Information >= pRequest->Length)
        {
            // reset data irq
            pRequest->DataReadyFlag &= ~BCM_I2C_REG_STATUS_TXW;
            InterruptMask &= ~BCM_I2C_REG_STATUS_TXW;
        }
    }

    //
    // Synchronize access to device context with ISR.
    //

    WdfInterruptAcquireLock(pDevice->InterruptObject);

    //
    // Set interrupt mask and clear current status.
    //
    PbcDeviceSetInterruptMask(pDevice, InterruptMask);

    pDevice->InterruptStatus = 0;

    Trace(
        TRACE_LEVEL_VERBOSE,
        TRACE_FLAG_TRANSFER,
        "Controller configured for %s of %Iu bytes to device 0x%lx (SPBREQUEST %p, WDFDEVICE %p)",
        pRequest->Direction == SpbTransferDirectionFromDevice ? "read" : "write",
        pRequest->Length,
        pDevice->pCurrentTarget->Settings.Address,
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
    // Check for address NACK.
    //

    if (TestAnyBits(InterruptStatus, BCM_I2C_REG_STATUS_ERR))
    {
        //
        // An address NACK indicates that a device is
        // not present at that address or is not responding.
        // Set the error status accordingly.
        //
        pRequest->Status = STATUS_NO_SUCH_DEVICE;
        pRequest->Information = 0;

        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_TRANSFER,
            "NACK on address 0x%lx (WDFDEVICE %p) - %!STATUS!",
            pDevice->pCurrentTarget->Settings.Address,
            pDevice->FxDevice,
            pRequest->Status);

        //
        // Complete the transfer and stop processing
        // interrupts.
        //

        ControllerCompleteTransfer(pDevice, pRequest, TRUE);
        goto exit;
    }

    //
    // Check for clock stretch timeout.
    //

    if (TestAnyBits(InterruptStatus, BCM_I2C_REG_STATUS_CLKT))
    {

        pRequest->Status = STATUS_UNSUCCESSFUL;

        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_FLAG_TRANSFER,
            "Error after %Iu bytes transferred for address 0x%lx "
            "(WDFDEVICE %p)- %!STATUS!",
            pRequest->Information,
            pDevice->pCurrentTarget->Settings.Address,
            pDevice->FxDevice,
            pRequest->Status);

        pRequest->Information = 0;

        //
        // Complete the transfer and stop processing
        // interrupts.
        //

        ControllerCompleteTransfer(pDevice, pRequest, TRUE);
        goto exit;
    }

    //
    // Check if controller is ready to transfer more data.
    //

    if (TestAnyBits(InterruptStatus, pRequest->DataReadyFlag))
    {
        //
        // Transfer data.
        //

        status = ControllerTransferData(pDevice, pRequest);

        if (!NT_SUCCESS(status))
        {
            pRequest->Status = status;

            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_TRANSFER,
                "Unexpected error while transferring data for address 0x%lx, "
                "completing transfer and resetting controller - %!STATUS!",
                pDevice->pCurrentTarget->Settings.Address,
                pRequest->Status);

            //
            // Complete the transfer and stop processing
            // interrupts.
            //

            ControllerCompleteTransfer(pDevice, pRequest, TRUE);
            goto exit;
        }

        //
        // If finished transferring data, stop listening for
        // data ready interrupt.  Do not complete transfer
        // until transfer complete interrupt occurs, unless 
        // the transfer is a repeated start.
        //

        if (PbcRequestGetInfoRemaining(pRequest) == 0)
        {
            Trace(
                TRACE_LEVEL_VERBOSE,
                TRACE_FLAG_TRANSFER,
                "No bytes remaining in transfer for address 0x%lx, wait for "
                "transfer complete interrupt",
                pDevice->pCurrentTarget->Settings.Address);

            if (pRequest->RepeatedStart)
            {
                ControllerCompleteTransfer(pDevice, pRequest, FALSE);
                goto exit;
            }
            else
            {
                PbcDeviceAndInterruptMask(pDevice, ~pRequest->DataReadyFlag);
            }
        }
    }

    //
    // Check if transfer is complete.
    //

    if (TestAnyBits(InterruptStatus, BCM_I2C_REG_STATUS_DONE))
    {
        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_TRANSFER,
            "Transfer complete for address 0x%lx with %Iu bytes remaining",
            pDevice->pCurrentTarget->Settings.Address,
            PbcRequestGetInfoRemaining(pRequest));

        //
        // If transfer complete interrupt occured and there
        // are still bytes remaining, transfer data. This occurs
        // when the number of bytes remaining is less than
        // the FIFO transfer level to trigger a data ready interrupt.
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
                    "Unexpected error while transferring data for address 0x%lx, "
                    "completing transfer and resetting controller "
                    "(WDFDEVICE %p) - %!STATUS!",
                    pDevice->pCurrentTarget->Settings.Address,
                    pDevice->FxDevice,
                    pRequest->Status);

                //
                // Complete the transfer and stop processing
                // interrupts.
                //

                ControllerCompleteTransfer(pDevice, pRequest, TRUE);
                goto exit;
            }
        }

        //
        // Complete the transfer.
        //

        ControllerCompleteTransfer(pDevice, pRequest, FALSE);
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

    size_t bytesToTransfer = min(pRequest->Length - pRequest->Information, BCM_I2C_MAX_BYTES_PER_TRANSFER);
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
            "Ready to write %Iu byte(s) for address 0x%lx",
            bytesToTransfer,
            pDevice->pCurrentTarget->Settings.Address);

        while (bytesTransferred < bytesToTransfer)
        {
            // Can Fifo accept more data?
            if (READ_REGISTER_ULONG(&pDevice->pRegisters->Status) & BCM_I2C_REG_STATUS_TXD)
            {
                status = PbcRequestGetByte(pRequest, pRequest->Information + bytesTransferred, &nextByte);
                if (NT_SUCCESS(status))
                {
                    WRITE_REGISTER_ULONG(&pDevice->pRegisters->DataFIFO, (ULONG)nextByte);
                    bytesTransferred++;
                }
                else
                {
                    NT_ASSERT(!"the buffer is too small or we wrote too many bytes");
                    break;
                }
            }
            else
            {
                break;
            }
        }
        pRequest->Information += bytesTransferred;
    }

    //
    // Read
    //

    else
    {

        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_TRANSFER,
            "Ready to read %Iu byte(s) for address 0x%lx",
            bytesToTransfer,
            pDevice->pCurrentTarget->Settings.Address);

        // read pending Rx data
        while (bytesTransferred < bytesToTransfer)
        {
            // Contains Rx Fifo data?
            if (READ_REGISTER_ULONG(&pDevice->pRegisters->Status) & BCM_I2C_REG_STATUS_RXD)
            {
                nextByte = (UCHAR)READ_REGISTER_ULONG(&pDevice->pRegisters->DataFIFO);
                status = PbcRequestSetByte(pRequest, pRequest->Information + bytesTransferred, nextByte);
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
                // fifo empty for this cycle, continue on next interrupt
                break;
            }
        }

        pRequest->Information += bytesTransferred;

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
        pDevice->pCurrentTarget->Settings.Address,
        pRequest->SpbRequest);

    //
    // Update request context with information from this transfer.
    //

    pRequest->TotalInformation += pRequest->Information;
    pRequest->Information = 0;

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

                PbcRequestDoTransfer(pDevice, pRequest);
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
    // Clear the controller's current target if any of
    //   1. request is type sequence
    //   2. request position is single 
    //      (did not come between lock/unlock)
    // Otherwise wait until unlock.
    //

    if ((pRequest->Type == SpbRequestTypeSequence) ||
        (pRequest->SequencePosition == SpbRequestSequencePositionSingle))
    {
        WRITE_REGISTER_ULONG(&pDevice->pRegisters->Control,
            pDevice->I2C_CONTROL_COPY | BCM_I2C_REG_CONTROL_CLEAR);
        pDevice->pCurrentTarget = NULL;
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
        pDevice->pCurrentTarget->Settings.Address);

    //
    // end the current transfer
    //

    WRITE_REGISTER_ULONG(&pDevice->pRegisters->Control,
        pDevice->I2C_CONTROL_COPY | BCM_I2C_REG_CONTROL_CLEAR);

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

    ULONG Control = pDevice->I2C_CONTROL_COPY;
    if (InterruptMask & BCM_I2C_REG_STATUS_RXR)
    {
        Control |= BCM_I2C_REG_CONTROL_INTR;
    }
    if (InterruptMask & BCM_I2C_REG_STATUS_TXW)
    {
        Control |= BCM_I2C_REG_CONTROL_INTT;
    }
    if (InterruptMask & BCM_I2C_REG_STATUS_DONE)
    {
        Control |= BCM_I2C_REG_CONTROL_INTD;
    }
    pDevice->I2C_CONTROL_COPY = Control;
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->Control, pDevice->I2C_CONTROL_COPY);

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

    pDevice->I2C_CONTROL_COPY &= (ULONG)~(
        BCM_I2C_REG_CONTROL_INTR | 
        BCM_I2C_REG_CONTROL_INTT |
        BCM_I2C_REG_CONTROL_INTD
        );
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->Control, pDevice->I2C_CONTROL_COPY);

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

    UNREFERENCED_PARAMETER(InterruptMask);

    ULONG interruptStatus;

    NT_ASSERT(pDevice != NULL);

    interruptStatus = READ_REGISTER_ULONG(&pDevice->pRegisters->Status);

    // mask r/w status if corresponding bit is not enabled
    interruptStatus &= InterruptMask | ~(BCM_I2C_REG_STATUS_RXR | BCM_I2C_REG_STATUS_TXW);

    // mask other unused bits in status to avoid spurios DPC
    interruptStatus &=
        BCM_I2C_REG_STATUS_TXW |
        BCM_I2C_REG_STATUS_RXR |
        BCM_I2C_REG_STATUS_DONE|
        BCM_I2C_REG_STATUS_ERR |
        BCM_I2C_REG_STATUS_CLKT;

    FuncExit(TRACE_FLAG_TRANSFER);

    return interruptStatus;
}

_Use_decl_annotations_
VOID
ControllerAcknowledgeInterrupts(
    PPBC_DEVICE   pDevice,
    ULONG         InterruptStatus
    )
/*++
 
  Routine Description:

    This routine acknowledges the
    specificed interrupt bits.

  Arguments:

    pDevice - a pointer to the PBC device context
    InterruptStatus - interrupt status bits to acknowledge

  Return Value:

    None.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NT_ASSERT(pDevice != NULL);

    // acknoledge and clear the DONE and ERROR status conditions
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->Status, InterruptStatus);

    FuncExit(TRACE_FLAG_TRANSFER);
}
