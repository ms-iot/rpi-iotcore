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

inline void
ControllerFlushFifos(
    _In_ PPBC_DEVICE pDevice
    )
/*++

    Routine Description:

        This routine waits until the Tx Fifo is flushed and clears the Rx Fifo

    Arguments:

        pDevice - a pointer to the PBC device context

    Return Value:

        None.

--*/
{
    // must only be called on active transfer
    NT_ASSERT(READ_REGISTER_ULONG(&pDevice->pSPIRegisters->CS) & BCM_SPI_REG_CS_TA);
    
    // clear the Rx fifo
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY | BCM_SPI_REG_CS_CLEARRX);

    // wait for Tx fifo empty
    ULONG timeout = BCM_SPI_FIFO_FLUSH_TIMEOUT_US;
    while ((timeout > 0) &&
           (READ_REGISTER_ULONG(&pDevice->pSPIRegisters->CS) & BCM_SPI_REG_CS_DONE) == 0)
    {   // do not flood the I/O bus
        KeStallExecutionProcessor(1);
        timeout--;
    }

    // clear the Rx fifo
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY | BCM_SPI_REG_CS_CLEARRX);

    if (timeout == 0)
    {
        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_FLAG_TRANSFER,
            "Flushing FIFOs timedout after %ul us for device 0x%lx (SPBREQUEST %p)",
            BCM_SPI_FIFO_FLUSH_TIMEOUT_US,
            pDevice->pCurrentTarget->Settings.DeviceSelection,
            pDevice->pCurrentTarget->pCurrentRequest->SpbRequest);
    }
}

_Use_decl_annotations_
VOID
ControllerInitialize(
    PPBC_DEVICE pDevice
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
    ControllerConfigClock(pDevice, BCM_SPI_REG_CLK_DEFAULT);

    FuncExit(TRACE_FLAG_PBCLOADING);
}

_Use_decl_annotations_
VOID
ControllerUninitialize(
    PPBC_DEVICE pDevice
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

    // make sure pending transactions are stopped
    ControllerDeactivateTransfer(pDevice);

    FuncExit(TRACE_FLAG_PBCLOADING);
}

_Use_decl_annotations_
NTSTATUS
ControllerDoOneTransferPollMode(
    PPBC_DEVICE pDevice,
    PPBC_REQUEST pRequest
    )
/*++
 
  Routine Description:

    This routine transfers data to or from the device in polling mode.

  Arguments:

    pDevice - a pointer to the PBC device context
    pRequest - a pointer to the PBC request context

  Return Value:

    None.

--*/
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    size_t bytesToWrite = 0;
    size_t bytesToRead = 0;
    NTSTATUS status = STATUS_SUCCESS;

    if (pRequest->CurrentTransferDelayInUs > 0)
    {
        status = ControllerDelayTransfer(pDevice, pRequest);
        if (!NT_SUCCESS(status))
        {
            goto exit;
        }
    }

    if (pRequest->CurrentTransferDirection == SpbTransferDirectionToDevice)
    {
        //
        // Write
        //

        bytesToWrite = pRequest->CurrentTransferWriteLength;

        Trace(
            TRACE_LEVEL_INFORMATION, 
            TRACE_FLAG_TRANSFER,
            "Ready to write %Iu byte(s) for device 0x%lx", 
            bytesToWrite,
            pDevice->pCurrentTarget->Settings.DeviceSelection);
    }
    else if (pRequest->CurrentTransferDirection == SpbTransferDirectionFromDevice)
    {
        //
        // Read
        //

        bytesToRead = pRequest->CurrentTransferReadLength;

        Trace(
            TRACE_LEVEL_INFORMATION, 
            TRACE_FLAG_TRANSFER,
            "Ready to read %Iu byte(s) for device 0x%lx", 
            bytesToRead,
            pDevice->pCurrentTarget->Settings.DeviceSelection);
    }
    else
    {
        //
        // full duplex
        //

        NT_ASSERT(pRequest->CurrentTransferDirection == SpbTransferDirectionNone);

        bytesToRead = pRequest->CurrentTransferReadLength;
        bytesToWrite = pRequest->CurrentTransferWriteLength;

        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_TRANSFER,
            "Ready to fullduplex write/read %Iu/%Iu byte(s) for device 0x%lx",
            bytesToWrite,
            bytesToRead,
            pDevice->pCurrentTarget->Settings.DeviceSelection);
    }

    size_t readByteIndex = 0;
    size_t writeByteIndex = 0;
    size_t transferByteLength = max(bytesToWrite, bytesToRead);
    size_t zeroBytesToWrite = transferByteLength - bytesToWrite;
    size_t readBytesToDiscard = transferByteLength - bytesToRead;
    UCHAR nextByte;
    ULONG CS;

    NT_ASSERT(
        (bytesToWrite + zeroBytesToWrite + bytesToRead + readBytesToDiscard) == 
        (transferByteLength * 2));

#ifdef DBG
    ULONGLONG numPolls = 0;
#endif

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Controller will use Polling on processor %u to transfer %Iu bytes  (SPBREQUEST %p, WDFDEVICE %p)",
        KeGetCurrentProcessorNumberEx(NULL),
        transferByteLength,
        pRequest->SpbRequest,
        pDevice->FxDevice);

    // As long as there are bytes to transfer and request has not been canceled
    while ((bytesToWrite > 0) ||
           (zeroBytesToWrite > 0) ||
           (bytesToRead > 0) ||
           (readBytesToDiscard > 0))
    {
        CS = READ_REGISTER_ULONG(&pDevice->pSPIRegisters->CS);

        if (CS & BCM_SPI_REG_CS_TXD)
        {
            // Write bytes to Tx FIFO from client buffer if any left
            // otherwise fill with zeros
            if (bytesToWrite)
            {
                status = MdlGetByte(
                    pRequest->pCurrentTransferWriteMdlChain,
                    writeByteIndex,
                    pRequest->CurrentTransferWriteLength,
                    &nextByte);
                if (!NT_SUCCESS(status))
                {
                    NT_ASSERTMSG("MDL size must match request set write buffer length", false);
                    status = STATUS_INVALID_PARAMETER;
                    goto exit;
                }

                WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO, nextByte);
                --bytesToWrite;
                ++writeByteIndex;
            }
            else if (zeroBytesToWrite)
            {
                WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO, 0);
                --zeroBytesToWrite;
            }
        }
        else if (WdfRequestIsCanceled(pRequest->SpbRequest))
        {
            //
            // If TX FIFO is full and can't take more bytes, then we 
            // have some free cycles to check if a request is cancelled 
            // and terminate transfer
            //

            status = STATUS_CANCELLED;

            Trace(
                TRACE_LEVEL_INFORMATION,
                TRACE_FLAG_TRANSFER,
                "Terminating transfer due to request cancelled SPBREQUEST %p",
                pRequest->SpbRequest);

            break;
        }

        if (CS & BCM_SPI_REG_CS_RXD)
        {
            // Read bvtes from Rx FIFO as long as there is a place in the client 
            // read buffer, otherwise discard read bytes
            if (bytesToRead > 0)
            {
                nextByte = (UCHAR)READ_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO);
                status = MdlSetByte(
                    pRequest->pCurrentTransferReadMdlChain, 
                    readByteIndex,
                    pRequest->CurrentTransferReadLength,
                    nextByte);
                if (!NT_SUCCESS(status))
                {
                    NT_ASSERTMSG("MDL size must match request set read buffer length", false);
                    status = STATUS_INVALID_PARAMETER;
                    goto exit;
                }

                --bytesToRead;
                ++readByteIndex;
            }
            else if (readBytesToDiscard > 0)
            {
                (void)READ_REGISTER_ULONG(&pDevice->pSPIRegisters->FIFO);
                --readBytesToDiscard;
            }
        }

    #ifdef DBG
        ++numPolls;
    #endif
    }

    ControllerFlushFifos(pDevice);

    pRequest->CurrentTransferInformation =
        (pRequest->CurrentTransferReadLength - bytesToRead) +
        (pRequest->CurrentTransferWriteLength - bytesToWrite);

#ifdef DBG
    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Polled %I64u time(s) to transfer %Iu byte(s)",
        numPolls,
        pRequest->CurrentTransferInformation);
#endif

exit:

    FuncExit(TRACE_FLAG_TRANSFER);
    
    return status;
}

_Use_decl_annotations_
bool
ControllerCompleteTransfer(
    PPBC_DEVICE pDevice,
    PPBC_REQUEST pRequest,
    NTSTATUS TransferStatus
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
        "Transfer (index %lu) with %!STATUS! with %Iu bytes for device 0x%lx (SPBREQUEST %p)",
        pRequest->CurrentTransferIndex,
        TransferStatus,
        pRequest->CurrentTransferInformation,
        pDevice->pCurrentTarget->Settings.DeviceSelection,
        pRequest->SpbRequest);

    //
    // Update request context with information from this transfer.
    //

    pRequest->TotalInformation += pRequest->CurrentTransferInformation;
    pRequest->CurrentTransferInformation = 0;

    //
    // Check if there are more transfers
    // in the sequence.
    //
    ++pRequest->CurrentTransferIndex;

    bool bIsRequestComplete = false;

    if ((pRequest->CurrentTransferIndex < pRequest->TransferCount) &&
        (TransferStatus != STATUS_CANCELLED))
    {
        goto exit;
    }

    //
    // end the current transfer if this was a single sequence or the last
    //

    // deassert cs only if not between lock / unlock pair
    if (!pDevice->Locked)
    {
        if ((pRequest->CurrentTransferSequencePosition == SpbRequestSequencePositionSingle) ||
            (pRequest->CurrentTransferSequencePosition == SpbRequestSequencePositionLast) ||
            (TransferStatus == STATUS_CANCELLED))
        {
            ControllerDeactivateTransfer(pDevice);
        }
    }

    pDevice->pCurrentTarget->pCurrentRequest = NULL;

    if (!pDevice->Locked)
    {
        //
        // Clear the controller's current target if any of
        //   1. request is type sequence or fullduplex
        //   2. request position is single 
        //      (did not come between lock/unlock)
        // Otherwise wait until unlock.
        //

        if ((pRequest->Type == SpbRequestTypeSequence) ||
            (pRequest->Type == SpbRequestTypeOther) ||
            (pRequest->CurrentTransferSequencePosition == SpbRequestSequencePositionSingle))
        {
            pDevice->pCurrentTarget = NULL;
        }
    }

    WdfRequestSetInformation(
        pRequest->SpbRequest,
        pRequest->TotalInformation);

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Completing request with %!STATUS! and Information=%Iu bytes (SPBREQUEST %p)",
        TransferStatus,
        pRequest->TotalInformation,
        pRequest->SpbRequest);

    SpbRequestComplete(
        pRequest->SpbRequest,
        TransferStatus);

    bIsRequestComplete = true;

exit:

    FuncExit(TRACE_FLAG_TRANSFER);

    return bIsRequestComplete;
}

_Use_decl_annotations_
VOID
ControllerUnlockTransfer(
    PPBC_DEVICE pDevice
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
        ControllerFlushFifos(pDevice);
        // stop transfer
        pDevice->SPI_CS_COPY &= ~BCM_SPI_REG_CS_TA;
        WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);
    }

    FuncExit(TRACE_FLAG_TRANSFER);
}

_Use_decl_annotations_
VOID
ControllerConfigForTargetAndActivate(
    PPBC_DEVICE pDevice
    )
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    PPBC_TARGET_SETTINGS pSettings = &pDevice->pCurrentTarget->Settings;

    if (pDevice->CurrentConnectionSpeed != pSettings->ConnectionSpeed)
    {
        pDevice->CurrentConnectionSpeed = pSettings->ConnectionSpeed;

        // set clock
        ControllerConfigClock(pDevice, pSettings->ConnectionSpeed);
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
    NT_ASSERT((pSettings->TypeSpecificFlags & SPI_WIREMODE_BIT) == 0);

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
    
    // reset Tx/Rx Fifos
    WRITE_REGISTER_ULONG(
        &pDevice->pSPIRegisters->CS,
        pDevice->SPI_CS_COPY | BCM_SPI_REG_CS_CLEARTX | BCM_SPI_REG_CS_CLEARRX);

    // start transfer
    ControllerActivateTransfer(pDevice);
    
    Trace(
        TRACE_LEVEL_VERBOSE,
        TRACE_FLAG_TRANSFER,
        "Controller configured for transfers to device on CS%hu (WDFDEVICE %p)",
        pDevice->pCurrentTarget->Settings.DeviceSelection,
        pDevice->FxDevice);

    FuncExit(TRACE_FLAG_TRANSFER);
}

_Use_decl_annotations_
NTSTATUS
ControllerDelayTransfer(
    PPBC_DEVICE pDevice,
    PPBC_REQUEST pRequest
    )
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    NTSTATUS status;

    if (pRequest->CurrentTransferDelayInUs == 0)
    {
        status = STATUS_SUCCESS;
        goto exit;
    }

    if (pRequest->CurrentTransferDelayInUs <= 1000)
    {
        //
        // Achieve high precision delay in us resolution by stalling
        //

        KeStallExecutionProcessor(pRequest->CurrentTransferDelayInUs);
        status = STATUS_SUCCESS;
    }
    else
    {
        //
        // TODO: Use WDF High Resolution Timer to implement delays in millisecond
        // resolution. Using standard WDF Timer will be a bad choice for delays < 15ms
        // See: https://msdn.microsoft.com/en-us/library/windows/hardware/ff545575(v=vs.85).aspx
        //

        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_TRANSFER,
            "Delaying more than 1ms is not implemented, requested delay %lu us WDFDEVICE %p",
            pRequest->CurrentTransferDelayInUs,
            pDevice->FxDevice);
        status = STATUS_NOT_SUPPORTED;
        goto exit;
    }

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Delayed %lu us before starting transfer for WDFDEVICE %p",
        pRequest->CurrentTransferDelayInUs,
        pDevice->FxDevice);

exit:

    FuncExit(TRACE_FLAG_TRANSFER);

    return status;
}

VOID
ControllerConfigClock(
    PPBC_DEVICE pDevice,
    ULONG clockHz
    )
{
    FuncEntry(TRACE_FLAG_TRANSFER);

    ULONG cdiv;

    if (clockHz <= BCM_SPI_CLK_MIN_HZ)
    {
        cdiv = BCM_SPI_REG_CLK_CDIV_MAX;
    }
    else if (clockHz >= BCM_SPI_CLK_MAX_HZ)
    {
        cdiv = BCM_SPI_REG_CLK_CDIV_MIN;
    }
    else
    {
        // 
        // There is a mistake in the datasheet that the
        // divider must be power of 2, it turns out that it
        // must be multiple of 2 i.e even number
        //

        cdiv = (BCM_APB_CLK / clockHz) & ULONG(~1);
    }

    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CLK, BCM_SPI_REG_CLK_CDIV_SET(cdiv));

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Configured SCLK, Asked:%uHz Given:%uHz usig CDIV=%u. WDFDEVICE %p",
        clockHz,
        BCM_APB_CLK / cdiv,
        cdiv,
        pDevice->FxDevice);

    FuncExit(TRACE_FLAG_TRANSFER);
}

VOID
ControllerActivateTransfer(
    PPBC_DEVICE pDevice)
{
    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Activating transfer");

    NT_ASSERT((pDevice->SPI_CS_COPY & BCM_SPI_REG_CS_TA) == 0);
    pDevice->SPI_CS_COPY |= BCM_SPI_REG_CS_TA;
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);
}

VOID
ControllerDeactivateTransfer(
    PPBC_DEVICE pDevice)
{
    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_TRANSFER,
        "Deactivating transfer");

    pDevice->SPI_CS_COPY &= ~BCM_SPI_REG_CS_TA;
    WRITE_REGISTER_ULONG(&pDevice->pSPIRegisters->CS, pDevice->SPI_CS_COPY);
}