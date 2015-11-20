/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    device.cpp

Abstract:

    This module contains WDF device initialization
    and SPB callback functions for the controller driver.

Environment:

    kernel-mode only

Revision History:

--*/

#include "precomp.h"

#include "i2ctrace.h"
#include "bcmi2c.h"
#include "driver.h"
#include "device.h"

#include "device.tmh"

#ifdef DBG
#define LOG_ISR_TIME 1
#endif // DBG

BCM_I2C_NONPAGED_SEGMENT_BEGIN; //=============================================

//
// Returns when the transfer becomes active, aborts due to an error,
// or times out.
//
// Returns:
//
//    STATUS_SUCCESS - The TA bit was set before the timeout.
//    STATUS_IO_TIMEOUT - A clock stretch timeout occurred.
//    STATUS_NO_SUCH_DEVICE - The slave address was not acknowledged.
//    STATUS_IO_DEVICE_ERROR - An unexpected hardware error occurred.
//
NTSTATUS WaitForTransferActive ( BCM_I2C_REGISTERS* RegistersPtr )
{
    ULONG count = 10000;  // maximum spin count
    do {
        ULONG statusReg = READ_REGISTER_NOFENCE_ULONG(&RegistersPtr->Status);
        if (statusReg & BCM_I2C_REG_STATUS_TA) {
            return STATUS_SUCCESS;
        } else if (statusReg & BCM_I2C_REG_STATUS_CLKT) {
            BSC_LOG_ERROR(
                L"CLKT was asserted while waiting for transfer to become active. (statusReg = 0x%lx)",
                statusReg);
            return STATUS_IO_TIMEOUT;
        } else if (statusReg & BCM_I2C_REG_STATUS_ERR) {
            BSC_LOG_ERROR(
                L"ERR was asserted while waiting for transfer to become active. (statusReg = 0x%lx)",
                statusReg);
            return STATUS_NO_SUCH_DEVICE;
        }
    } while (--count);

    BSC_LOG_ERROR(L"Maximum spin count reached waiting for transfer to become active.");
    return STATUS_IO_TIMEOUT;
}

void InitializeTransfer (
    BCM_I2C_REGISTERS* RegistersPtr,
    const BCM_I2C_TARGET_CONTEXT* TargetPtr,
    ULONG DataLength
    )
{
    WRITE_REGISTER_NOFENCE_ULONG(
        &RegistersPtr->Control,
        BCM_I2C_REG_CONTROL_CLEAR);

    // Clear error and done
    WRITE_REGISTER_NOFENCE_ULONG(
        &RegistersPtr->Status,
        BCM_I2C_REG_STATUS_CLKT |
        BCM_I2C_REG_STATUS_ERR |
        BCM_I2C_REG_STATUS_DONE);

    // program clock speed
    NT_ASSERT(
        (TargetPtr->ConnectionSpeed >= BCM_I2C_MIN_CONNECTION_SPEED) &&
        (TargetPtr->ConnectionSpeed <= BCM_I2C_MAX_CONNECTION_SPEED));
    ULONG clockDivider =
        (BCM_I2C_CORE_CLOCK / TargetPtr->ConnectionSpeed) &
         BCM_I2C_REG_CDIV_MASK;
    WRITE_REGISTER_NOFENCE_ULONG(&RegistersPtr->ClockDivider, clockDivider);

    //
    // The rising edge data delay sets how long the controller waits after
    // a rising edge before sampling the incoming data. With the default value
    // of 0x30, corruption was seen in the first bit of received data with
    // a device that does clock stretching. Increasing REDL gives the slave
    // device more time to pull the line low or let it rise high. Increasing
    // REDL solved the corruption. REDL must be less than CDIV / 2.
    // 50 is a safety margin to ensure REDL is less than CDIV / 2.
    //
    NT_ASSERT((clockDivider / 2) > 50);
    WRITE_REGISTER_NOFENCE_ULONG(
        &RegistersPtr->DataDelay,
        (BCM_I2C_REG_DEL_FEDL << 16) | (clockDivider / 2 - 50));

    // program slave address
    NT_ASSERT(TargetPtr->Address <= I2C_MAX_ADDRESS);
    WRITE_REGISTER_NOFENCE_ULONG(
        &RegistersPtr->SlaveAddress,
        TargetPtr->Address & BCM_I2C_REG_ADDRESS_MASK);

    // Program data length
    NT_ASSERT(DataLength <= BCM_I2C_MAX_TRANSFER_LENGTH);
    WRITE_REGISTER_NOFENCE_ULONG(
        &RegistersPtr->DataLength,
        DataLength & BCM_I2C_REG_DLEN_MASK);
}

//
// Reads up to the specified number of bytes from the data FIFO. Returns when
// either all available bytes have been read or all requested bytes have
// been read. Returns the number of bytes read.
//
ULONG ReadFifo (
    BCM_I2C_REGISTERS* RegistersPtr,
    _Out_writes_to_(BufferSize, return) BYTE* BufferPtr,
    ULONG BufferSize
    )
{
    BYTE* dataPtr = BufferPtr;
    while (dataPtr != (BufferPtr + BufferSize)) {
        ULONG statusReg = READ_REGISTER_NOFENCE_ULONG(&RegistersPtr->Status);
        if (!(statusReg & BCM_I2C_REG_STATUS_RXD)) {
            break;
        }

        *dataPtr++ = static_cast<BYTE>(
            READ_REGISTER_NOFENCE_ULONG(&RegistersPtr->DataFIFO));
    }

    ULONG bytesRead = dataPtr - BufferPtr;
    BSC_LOG_TRACE(
        "Read %d of %d bytes from RX FIFO",
        bytesRead,
        BufferSize);

    return bytesRead;
}

ULONG ReadFifoMdl (BCM_I2C_REGISTERS* RegistersPtr, PMDL Mdl, ULONG Offset)
{
    NT_ASSERT(Offset <= MmGetMdlByteCount(Mdl));
    NT_ASSERT(
        Mdl->MdlFlags &
       (MDL_MAPPED_TO_SYSTEM_VA | MDL_SOURCE_IS_NONPAGED_POOL));

    return ReadFifo(
        RegistersPtr,
        static_cast<BYTE*>(Mdl->MappedSystemVa) + Offset,
        MmGetMdlByteCount(Mdl) - Offset);
}

//
// Writes up to the specified number of bytes to the data FIFO. Returns when
// either the FIFO is full or the entire buffer has been written. Returns
// the number of bytes written to the FIFO.
//
ULONG WriteFifo (
    BCM_I2C_REGISTERS* RegistersPtr,
    _In_reads_(BufferSize) const BYTE* BufferPtr,
    ULONG BufferSize
    )
{
    const BYTE* dataPtr = BufferPtr;
    while (dataPtr != (BufferPtr + BufferSize)) {
        ULONG statusReg = READ_REGISTER_NOFENCE_ULONG(&RegistersPtr->Status);
        if (!(statusReg & BCM_I2C_REG_STATUS_TXD)) {
            break;
        }

        WRITE_REGISTER_NOFENCE_ULONG(&RegistersPtr->DataFIFO, *dataPtr++);
    }

    ULONG bytesWritten = dataPtr - BufferPtr;
    BSC_LOG_TRACE(
        "Wrote %d of %d bytes to TX FIFO",
        bytesWritten,
        BufferSize);

    return bytesWritten;
}

_Use_decl_annotations_
VOID OnRead (
    WDFDEVICE WdfDevice,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest,
    size_t Length
    )
{
    BCM_I2C_ASSERT_MAX_IRQL(DISPATCH_LEVEL);

    NTSTATUS status;

    BYTE* readBufferPtr;
    ULONG bytesToRead;
    {
        PVOID outputBufferPtr;
        size_t outputBufferLength;
        status = WdfRequestRetrieveOutputBuffer(
                    SpbRequest,
                    1,          // MinimumRequiredSize
                    &outputBufferPtr,
                    &outputBufferLength);
        if (!NT_SUCCESS(status)) {
            BSC_LOG_ERROR(
                "Failed to retreive output buffer from request. (SpbRequest = %p, status = %!STATUS!)",
                SpbRequest,
                status);
            SpbRequestComplete(SpbRequest, status);
            return;
        }

        UNREFERENCED_PARAMETER(Length);
        NT_ASSERT(outputBufferLength == Length);
        if (outputBufferLength > BCM_I2C_MAX_TRANSFER_LENGTH) {
            BSC_LOG_ERROR(
                "Output buffer is too large for DataLength register. (SpbRequest = %p, outputBufferLength = %lu, BCM_I2C_MAX_TRANSFER_LENGTH = %lu)",
                SpbRequest,
                outputBufferLength,
                BCM_I2C_MAX_TRANSFER_LENGTH);
            SpbRequestComplete(SpbRequest, STATUS_NOT_SUPPORTED);
            return;
        }

        readBufferPtr = static_cast<BYTE*>(outputBufferPtr);
        bytesToRead = static_cast<ULONG>(outputBufferLength);
    }

    BCM_I2C_DEVICE_CONTEXT* devicePtr = GetDeviceContext(WdfDevice);
    BCM_I2C_REGISTERS* registersPtr = devicePtr->RegistersPtr;

    // get connection settings
    const BCM_I2C_TARGET_CONTEXT* targetPtr = GetTargetContext(SpbTarget);

    BSC_LOG_TRACE(
        "Setting up Read request. (targetPtr->Address = 0x%lx, targetPtr->ConnectionSpeed = %lu, readBufferPtr = %p, bytesToRead = %lu)",
        targetPtr->Address,
        targetPtr->ConnectionSpeed,
        readBufferPtr,
        bytesToRead);

    InitializeTransfer(registersPtr, targetPtr, bytesToRead);

    // Start transfer
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Control,
        BCM_I2C_REG_CONTROL_I2CEN |
        BCM_I2C_REG_CONTROL_ST |
        BCM_I2C_REG_CONTROL_CLEAR |
        BCM_I2C_REG_CONTROL_READ);

    // Must set up interrupt context before marking request cancelable
    BCM_I2C_INTERRUPT_CONTEXT* interruptContextPtr;
    {
        interruptContextPtr = devicePtr->InterruptContextPtr;
        NT_ASSERT(interruptContextPtr->RegistersPtr == registersPtr);

        interruptContextPtr->SpbRequest = SpbRequest;
        interruptContextPtr->TargetPtr = targetPtr;
        interruptContextPtr->State = TRANSFER_STATE::RECEIVING;
        interruptContextPtr->CapturedStatus = 0;
        interruptContextPtr->CapturedDataLength = 0;

        interruptContextPtr->ReadContext.ReadBufferPtr = readBufferPtr;
        interruptContextPtr->ReadContext.CurrentReadBufferPtr = readBufferPtr;
        interruptContextPtr->ReadContext.EndPtr = readBufferPtr + bytesToRead;
    }

    status = WdfRequestMarkCancelableEx(SpbRequest, OnRequestCancel);
    if (!NT_SUCCESS(status)) {
        BSC_LOG_INFORMATION(
            "Failed to mark request cancelable. (SpbRequest = %p, status = %!STATUS!)",
            SpbRequest,
            status);
        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->Control,
            BCM_I2C_REG_CONTROL_I2CEN | BCM_I2C_REG_CONTROL_CLEAR);
        SpbRequestComplete(SpbRequest, status);
        return;
    }

    BSC_LOG_TRACE("Finished setting up the read, enabling interrupts");

    // enable interrupts
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Control,
        BCM_I2C_REG_CONTROL_I2CEN |
        BCM_I2C_REG_CONTROL_INTR |
        BCM_I2C_REG_CONTROL_INTD |
        BCM_I2C_REG_CONTROL_READ);
}

_Use_decl_annotations_
VOID OnWrite (
    WDFDEVICE WdfDevice,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest,
    size_t Length
    )
{
    BCM_I2C_ASSERT_MAX_IRQL(DISPATCH_LEVEL);

    NTSTATUS status;

    const BYTE* writeBufferPtr;
    ULONG bytesToWrite;
    {
        PVOID inputBufferPtr;
        size_t inputBufferLength;
        status = WdfRequestRetrieveInputBuffer(
                    SpbRequest,
                    1,          // MinimumRequiredSize
                    &inputBufferPtr,
                    &inputBufferLength);
        if (!NT_SUCCESS(status)) {
            BSC_LOG_ERROR(
                "Failed to retrieve input buffer from request. (SpbRequest = %p, status = %!STATUS!)",
                SpbRequest,
                status);
            SpbRequestComplete(SpbRequest, status);
            return;
        }

        UNREFERENCED_PARAMETER(Length);
        NT_ASSERT(inputBufferLength == Length);
        if (inputBufferLength > BCM_I2C_MAX_TRANSFER_LENGTH) {
            BSC_LOG_ERROR(
                "Write buffer is too large. (SpbRequest = %p, inputBufferLength = %lu, BCM_I2C_MAX_TRANSFER_LENGTH = %lu)",
                SpbRequest,
                inputBufferLength,
                BCM_I2C_MAX_TRANSFER_LENGTH);
            SpbRequestComplete(SpbRequest, STATUS_NOT_SUPPORTED);
            return;
        }

        writeBufferPtr = static_cast<const BYTE*>(inputBufferPtr);
        bytesToWrite = static_cast<ULONG>(inputBufferLength);
    }

    BCM_I2C_DEVICE_CONTEXT* devicePtr = GetDeviceContext(WdfDevice);
    BCM_I2C_REGISTERS* registersPtr = devicePtr->RegistersPtr;

    // get connection settings
    const BCM_I2C_TARGET_CONTEXT* targetPtr = GetTargetContext(SpbTarget);

    BSC_LOG_TRACE(
        "Setting up Write request. (targetPtr->Address = 0x%lx, targetPtr->ConnectionSpeed = %lu, writeBufferPtr = %p, bytesToWrite = %lu)",
        targetPtr->Address,
        targetPtr->ConnectionSpeed,
        writeBufferPtr,
        bytesToWrite);

    InitializeTransfer(registersPtr, targetPtr, bytesToWrite);

    // Start transfer
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Control,
        BCM_I2C_REG_CONTROL_I2CEN |
        BCM_I2C_REG_CONTROL_ST |
        BCM_I2C_REG_CONTROL_CLEAR);

    // do initial fill of FIFO
    ULONG bytesWritten = WriteFifo(
                            registersPtr,
                            writeBufferPtr,
                            bytesToWrite);

    // set up interrupt context
    BCM_I2C_INTERRUPT_CONTEXT* interruptContextPtr;
    {
        interruptContextPtr = devicePtr->InterruptContextPtr;
        NT_ASSERT(interruptContextPtr->RegistersPtr == registersPtr);

        interruptContextPtr->SpbRequest = SpbRequest;
        interruptContextPtr->TargetPtr = targetPtr;
        interruptContextPtr->State = TRANSFER_STATE::SENDING;
        interruptContextPtr->CapturedStatus = 0;
        interruptContextPtr->CapturedDataLength = 0;

        interruptContextPtr->WriteContext.WriteBufferPtr = writeBufferPtr;
        interruptContextPtr->WriteContext.CurrentWriteBufferPtr =
            writeBufferPtr + bytesWritten;
        interruptContextPtr->WriteContext.EndPtr =
            writeBufferPtr + bytesToWrite;
    }

    status = WdfRequestMarkCancelableEx(SpbRequest, OnRequestCancel);
    if (!NT_SUCCESS(status)) {
        BSC_LOG_INFORMATION(
            "Failed to mark request cancelable. (SpbRequest = %p, status = %!STATUS!)",
            SpbRequest,
            status);
        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->Control,
            BCM_I2C_REG_CONTROL_I2CEN | BCM_I2C_REG_CONTROL_CLEAR);
        SpbRequestComplete(SpbRequest, status);
        return;
    }

    BSC_LOG_TRACE("Finished setting up the write, enabling interrupts");

    // enable interrupts
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Control,
        BCM_I2C_REG_CONTROL_I2CEN |
        BCM_I2C_REG_CONTROL_INTT |
        BCM_I2C_REG_CONTROL_INTD);
}

//
// Does the initial write of a sequence transfer.
//
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS StartSequenceWrite ( BCM_I2C_INTERRUPT_CONTEXT* InterruptContextPtr )
{
    BCM_I2C_ASSERT_MAX_IRQL(DISPATCH_LEVEL);

    NTSTATUS status;
    BCM_I2C_INTERRUPT_CONTEXT::SEQUENCE_CONTEXT* sequenceContextPtr =
        &InterruptContextPtr->SequenceContext;
    BCM_I2C_REGISTERS* registersPtr = InterruptContextPtr->RegistersPtr;
    SPBREQUEST spbRequest = InterruptContextPtr->SpbRequest;

    NT_ASSERT(InterruptContextPtr->State == TRANSFER_STATE::SENDING_SEQUENCE);
    NT_ASSERT(sequenceContextPtr->BytesToWrite != 0);

    if (sequenceContextPtr->BytesToWrite == 1) {
        BSC_LOG_TRACE(
            "Transmit buffer is length 1; waiting for transfer to become active and then setting up the read. (bytesToRead = %lu)",
            sequenceContextPtr->BytesToRead);

        status = WaitForTransferActive(registersPtr);
        if (!NT_SUCCESS(status)) {
            BSC_LOG_ERROR(
                "The transfer failed to become active. (status = %!STATUS!)",
                status);
            return status;
        }

        status = WdfRequestMarkCancelableEx(spbRequest, OnRequestCancel);
        if (!NT_SUCCESS(status)) {
            BSC_LOG_INFORMATION(
                "Failed to mark request cancelable. (spbRequest = %p, status = %!STATUS!)",
                spbRequest,
                status);
            return status;
        }

        //
        // This lock prevents preemption by the ISR when queuing the first
        // byte to the data FIFO. The control register cannot be written to
        // again after the read is programmed, so interrupts must be enabled in
        // the same register operation.
        //
        {
            WdfInterruptAcquireLock(InterruptContextPtr->WdfInterrupt);

            WRITE_REGISTER_NOFENCE_ULONG(
                &registersPtr->DataLength,
                sequenceContextPtr->BytesToRead);
            WRITE_REGISTER_NOFENCE_ULONG(
                &registersPtr->Control,
                BCM_I2C_REG_CONTROL_I2CEN |
                BCM_I2C_REG_CONTROL_ST |
                BCM_I2C_REG_CONTROL_INTR |
                BCM_I2C_REG_CONTROL_INTD |
                BCM_I2C_REG_CONTROL_READ);

            // write first and only byte to FIFO
            NT_ASSERT(
                sequenceContextPtr->CurrentWriteMdl->MdlFlags &
               (MDL_MAPPED_TO_SYSTEM_VA | MDL_SOURCE_IS_NONPAGED_POOL));
            WRITE_REGISTER_NOFENCE_ULONG(
                &registersPtr->DataFIFO,
                *static_cast<const BYTE*>(
                    sequenceContextPtr->CurrentWriteMdl->MappedSystemVa));
            ++sequenceContextPtr->BytesWritten;
            ++sequenceContextPtr->CurrentWriteMdlOffset;

            NT_ASSERT(
                sequenceContextPtr->BytesWritten ==
                sequenceContextPtr->BytesToWrite);
            NT_ASSERT(sequenceContextPtr->CurrentWriteMdlOffset == 1);

            NT_ASSERT(!sequenceContextPtr->CurrentWriteMdl->Next);
            sequenceContextPtr->CurrentWriteMdl = nullptr;
            InterruptContextPtr->State = TRANSFER_STATE::RECEIVING_SEQUENCE;

            WdfInterruptReleaseLock(InterruptContextPtr->WdfInterrupt);
        }
    } else {
        NT_ASSERT(sequenceContextPtr->BytesToWrite > 1);
        BSC_LOG_TRACE("Transmit buffer is 2 or greater, writing first byte and enabling TXW interrupt.");

        status = WdfRequestMarkCancelableEx(spbRequest, OnRequestCancel);
        if (!NT_SUCCESS(status)) {
            BSC_LOG_INFORMATION(
                "Failed to mark request cancelable. (spbRequest = %p, status = %!STATUS!)",
                spbRequest,
                status);
            return status;
        }

        // write first byte to FIFO
        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->DataFIFO,
            *static_cast<const BYTE*>(
                sequenceContextPtr->CurrentWriteMdl->MappedSystemVa));
        ++sequenceContextPtr->BytesWritten;
        ++sequenceContextPtr->CurrentWriteMdlOffset;

        NT_ASSERT(sequenceContextPtr->BytesWritten == 1);
        NT_ASSERT(sequenceContextPtr->CurrentWriteMdlOffset == 1);

        // enable interrupts
        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->Control,
            BCM_I2C_REG_CONTROL_I2CEN |
            BCM_I2C_REG_CONTROL_INTT |
            BCM_I2C_REG_CONTROL_INTD);
    }

    return STATUS_SUCCESS;
}

//
// The Broadcom I2C controller does not support arbitrary restarts; it only
// supports a single WriteRead sequence.
//
_Use_decl_annotations_
VOID OnSequence (
    WDFDEVICE WdfDevice,
    SPBTARGET SpbTarget,
    SPBREQUEST SpbRequest,
    ULONG TransferCount
    )
{
    BCM_I2C_ASSERT_MAX_IRQL(DISPATCH_LEVEL);

    if (TransferCount != 2) {
        BSC_LOG_ERROR(
            "Unsupported sequence attempted. Broadcom I2C controller only supports WriteRead sequences. (TransferCount = %lu)",
            TransferCount);
        SpbRequestComplete(SpbRequest, STATUS_NOT_SUPPORTED);
        return;
    }

    SPB_TRANSFER_DESCRIPTOR writeDescriptor;
    SPB_TRANSFER_DESCRIPTOR readDescriptor;
    PMDL writeMdl;
    ULONG bytesToWrite;
    PMDL readMdl;
    ULONG bytesToRead;
    {
        SPB_TRANSFER_DESCRIPTOR_INIT(&writeDescriptor);
        SpbRequestGetTransferParameters(
            SpbRequest,
            0,
            &writeDescriptor,
            &writeMdl);

        // validate first transfer descriptor to make sure it's a write
        if ((writeDescriptor.Direction != SpbTransferDirectionToDevice)) {
            BSC_LOG_ERROR(
                "Unsupported sequence attempted. The first transfer must be a write. (writeDescriptor.Direction = %d)",
                writeDescriptor.Direction);
            SpbRequestComplete(SpbRequest, STATUS_NOT_SUPPORTED);
            return;
        }

        if (writeDescriptor.TransferLength > BCM_I2C_MAX_TRANSFER_LENGTH) {
            BSC_LOG_ERROR(
                "Write buffer is too large. (SpbRequest = %p, writeDescriptor.TransferLength = %lu, BCM_I2C_MAX_TRANSFER_LENGTH = %lu)",
                SpbRequest,
                writeDescriptor.TransferLength,
                BCM_I2C_MAX_TRANSFER_LENGTH);
            SpbRequestComplete(SpbRequest, STATUS_NOT_SUPPORTED);
            return;
        }

        if (writeDescriptor.DelayInUs != 0) {
            BSC_LOG_ERROR(
                "Delays are not supported. (writeDescriptor.DelayInUs = %lu)",
                writeDescriptor.DelayInUs);
            SpbRequestComplete(SpbRequest, STATUS_NOT_SUPPORTED);
            return;
        }

        // validate second transfer descriptor to make sure it's a read
        SPB_TRANSFER_DESCRIPTOR_INIT(&readDescriptor);
        SpbRequestGetTransferParameters(
            SpbRequest,
            1,
            &readDescriptor,
            &readMdl);
        if (readDescriptor.Direction != SpbTransferDirectionFromDevice) {
            BSC_LOG_ERROR(
                "Unsupported sequence attempted. The second transfer must be a read. (readDescriptor.Direction = %d)",
                readDescriptor.Direction);
            SpbRequestComplete(SpbRequest, STATUS_NOT_SUPPORTED);
            return;
        }

        if (readDescriptor.TransferLength > BCM_I2C_MAX_TRANSFER_LENGTH) {
            BSC_LOG_ERROR(
                "Read buffer is too large for DataLength register. (SpbRequest = %p, readDescriptor.TransferLength= %lu, BCM_I2C_MAX_TRANSFER_LENGTH = %lu)",
                SpbRequest,
                readDescriptor.TransferLength,
                BCM_I2C_MAX_TRANSFER_LENGTH);
            SpbRequestComplete(SpbRequest, STATUS_NOT_SUPPORTED);
            return;
        }

        if (readDescriptor.DelayInUs != 0) {
            BSC_LOG_ERROR(
                "BCM I2C controller is not capable of having a delay in a read transaction. (readDescriptor.DelayInUs = %lu)",
                readDescriptor.DelayInUs);
            SpbRequestComplete(SpbRequest, STATUS_NOT_SUPPORTED);
            return;
        }

        bytesToWrite = 0;
        for (PMDL currentMdl = writeMdl;
             currentMdl;
             currentMdl = currentMdl->Next) {

            const PVOID ptr = MmGetSystemAddressForMdlSafe(
                currentMdl,
                NormalPagePriority | MdlMappingNoWrite | MdlMappingNoExecute);
            if (!ptr) {
                BSC_LOG_LOW_MEMORY(
                    "MmGetSystemAddressForMdlSafe() failed. (currentMdl = %p)",
                    currentMdl);
                SpbRequestComplete(SpbRequest, STATUS_INSUFFICIENT_RESOURCES);
                return;
            }
            NT_ASSERT(MmGetMdlByteCount(currentMdl) != 0);
            bytesToWrite += MmGetMdlByteCount(currentMdl);
        }

        NT_ASSERT(bytesToWrite == writeDescriptor.TransferLength);

        bytesToRead = 0;
        for (PMDL currentMdl = readMdl;
             currentMdl;
             currentMdl = currentMdl->Next) {

            PVOID ptr = MmGetSystemAddressForMdlSafe(
                currentMdl,
                NormalPagePriority | MdlMappingNoExecute);
            if (!ptr) {
                BSC_LOG_LOW_MEMORY(
                    "MmGetSystemAddressForMdlSafe() failed. (currentMdl = %p)",
                    currentMdl);
                SpbRequestComplete(SpbRequest, STATUS_INSUFFICIENT_RESOURCES);
                return;
            }

            NT_ASSERT(MmGetMdlByteCount(currentMdl) != 0);
            bytesToRead += MmGetMdlByteCount(currentMdl);
        }

        NT_ASSERT(bytesToRead == readDescriptor.TransferLength);
    }

    BCM_I2C_DEVICE_CONTEXT* devicePtr = GetDeviceContext(WdfDevice);
    BCM_I2C_INTERRUPT_CONTEXT* interruptContextPtr;
    {
        interruptContextPtr = devicePtr->InterruptContextPtr;
        interruptContextPtr->SpbRequest = SpbRequest;
        interruptContextPtr->TargetPtr = GetTargetContext(SpbTarget);
        interruptContextPtr->State = TRANSFER_STATE::SENDING_SEQUENCE;
        interruptContextPtr->CapturedStatus = 0;
        interruptContextPtr->CapturedDataLength = 0;

        BCM_I2C_INTERRUPT_CONTEXT::SEQUENCE_CONTEXT* sequenceContextPtr =
            &interruptContextPtr->SequenceContext;
        sequenceContextPtr->CurrentWriteMdl = writeMdl;
        sequenceContextPtr->BytesToWrite = bytesToWrite;
        sequenceContextPtr->BytesWritten = 0;
        sequenceContextPtr->CurrentWriteMdlOffset = 0;

        sequenceContextPtr->CurrentReadMdl = readMdl;
        sequenceContextPtr->BytesToRead = bytesToRead;
        sequenceContextPtr->BytesRead = 0;
        sequenceContextPtr->CurrentReadMdlOffset = 0;
    }

    BSC_LOG_TRACE(
        "Setting up and starting Write portion of WriteRead transfer. (Address = 0x%x, ConnectionSpeed = %lu, bytesToWrite = %lu)",
        interruptContextPtr->TargetPtr->Address,
        interruptContextPtr->TargetPtr->ConnectionSpeed,
        bytesToWrite);

    BCM_I2C_REGISTERS* registersPtr = devicePtr->RegistersPtr;
    InitializeTransfer(
        registersPtr,
        interruptContextPtr->TargetPtr,
        bytesToWrite);

    // start transfer
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Control,
        BCM_I2C_REG_CONTROL_I2CEN |
        BCM_I2C_REG_CONTROL_ST |
        BCM_I2C_REG_CONTROL_CLEAR);

    NTSTATUS status = StartSequenceWrite(interruptContextPtr);
    if (!NT_SUCCESS(status)) {
        BSC_LOG_ERROR(
            "Failed to do initial write of the sequence transfer. (status = %!STATUS!)",
            status);
        // abort the transfer since it has already been started
        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->Control,
            BCM_I2C_REG_CONTROL_I2CEN | BCM_I2C_REG_CONTROL_CLEAR);
        SpbRequestComplete(SpbRequest, status);
        return;
    }
}

_Use_decl_annotations_
VOID OnRequestCancel ( WDFREQUEST  WdfRequest )
{
    BCM_I2C_ASSERT_MAX_IRQL(DISPATCH_LEVEL);

    BCM_I2C_DEVICE_CONTEXT* devicePtr = GetDeviceContext(WdfFileObjectGetDevice(
        WdfRequestGetFileObject(WdfRequest)));
    BCM_I2C_REGISTERS* registersPtr = devicePtr->RegistersPtr;
    BCM_I2C_INTERRUPT_CONTEXT* interruptContextPtr =
        devicePtr->InterruptContextPtr;

    BSC_LOG_INFORMATION(
        "Cancellation requested. (WdfRequest = %p, interruptContextPtr = %p)",
        WdfRequest,
        interruptContextPtr);

    // synchronize with ISR to ensure interrupts get disabled
    {
        WdfInterruptAcquireLock(devicePtr->WdfInterrupt);

        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->Control,
            BCM_I2C_REG_CONTROL_I2CEN | BCM_I2C_REG_CONTROL_CLEAR);
        WRITE_REGISTER_NOFENCE_ULONG(&registersPtr->DataLength, 0);

        // clear DONE and ERROR bits
        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->Status,
            BCM_I2C_REG_STATUS_ERR |
            BCM_I2C_REG_STATUS_CLKT |
            BCM_I2C_REG_STATUS_DONE);

        WdfInterruptReleaseLock(devicePtr->WdfInterrupt);
    }

    // get the request from the interrupt context
    SPBREQUEST currentRequest = static_cast<SPBREQUEST>(
        InterlockedExchangePointer(
            reinterpret_cast<PVOID *>(&interruptContextPtr->SpbRequest),
            nullptr));
    if (!currentRequest) {
        BSC_LOG_TRACE("Cannot cancel request, it was already claimed by DPC.");
        return;
    }
    NT_ASSERT(WdfRequest == currentRequest);
    SpbRequestComplete(static_cast<SPBREQUEST>(WdfRequest), STATUS_CANCELLED);
}

BOOLEAN HandleInterrupt ( WDFINTERRUPT WdfInterrupt )
{
    BCM_I2C_INTERRUPT_CONTEXT* interruptContextPtr =
        GetInterruptContext(WdfInterrupt);
    BCM_I2C_REGISTERS* registersPtr = interruptContextPtr->RegistersPtr;

    ULONG statusReg = READ_REGISTER_NOFENCE_ULONG(&registersPtr->Status);

#ifdef DBG
    BSC_LOG_TRACE("Interrupt occurred. (statusReg = 0x%lx)", statusReg);
#endif // DBG

    if (!(statusReg & (BCM_I2C_REG_STATUS_RXR | BCM_I2C_REG_STATUS_TXW |
                       BCM_I2C_REG_STATUS_DONE))) {

        BSC_LOG_TRACE("Interrupt bits were not set - not claiming interrupt");
        return FALSE;
    } else if (statusReg & (BCM_I2C_REG_STATUS_CLKT | BCM_I2C_REG_STATUS_ERR |
                            BCM_I2C_REG_STATUS_DONE)) {

        // capture data length before writing to any registers
        ULONG dataLength =
            READ_REGISTER_NOFENCE_ULONG(&registersPtr->DataLength);

        bool queueDpc;
        if (statusReg & (BCM_I2C_REG_STATUS_CLKT | BCM_I2C_REG_STATUS_ERR)) {
            BSC_LOG_TRACE(
                "One of the error bits is set. (statusReg = 0x%lx, dataLength = %lu)",
                statusReg,
                dataLength);
            queueDpc = true;
        } else {
            NT_ASSERT(statusReg & BCM_I2C_REG_STATUS_DONE);

            BSC_LOG_TRACE(
                "DONE bit was set - acknowledging DONE bit and checking TA. (statusReg = 0x%lx, dataLength = %lu)",
                statusReg,
                dataLength);

            WRITE_REGISTER_NOFENCE_ULONG(
                &registersPtr->Status,
                BCM_I2C_REG_STATUS_DONE);

            //
            // DONE sometimes gets set spuriously. Check if transfer is still
            // active. If transfer is still active, DONE will be asserted again
            // when the transfer completes. If transfer is not still active
            // after acknowledging DONE bit, go to DPC.
            //
            queueDpc = !(READ_REGISTER_NOFENCE_ULONG(&registersPtr->Status) &
                         BCM_I2C_REG_STATUS_TA);
        }

        if (queueDpc) {
            interruptContextPtr->CapturedStatus = statusReg;
            interruptContextPtr->CapturedDataLength = dataLength;

            BSC_LOG_TRACE(
                "Going to DPC. (interruptContextPtr->CapturedStatus = 0x%lx, interruptContextPtr->CapturedDataLength = %lu)",
                interruptContextPtr->CapturedStatus,
                interruptContextPtr->CapturedDataLength);

            // disable interrupts, preserving the READ flag
            ULONG controlReg =
                READ_REGISTER_NOFENCE_ULONG(&registersPtr->Control);
            WRITE_REGISTER_NOFENCE_ULONG(
                &registersPtr->Control,
                BCM_I2C_REG_CONTROL_I2CEN |
                (controlReg & BCM_I2C_REG_CONTROL_READ));
            WdfInterruptQueueDpcForIsr(WdfInterrupt);
            return TRUE;
        }
    }

    NT_ASSERTMSG(
        "Expecting a current request",
        interruptContextPtr->SpbRequest);

    switch (interruptContextPtr->State) {
    case TRANSFER_STATE::SENDING:
    {
        BCM_I2C_INTERRUPT_CONTEXT::WRITE_CONTEXT* writeContextPtr =
            &interruptContextPtr->WriteContext;

        const BYTE* dataPtr = writeContextPtr->CurrentWriteBufferPtr;
        const BYTE* const endPtr = writeContextPtr->EndPtr;
        NT_ASSERTMSG(
            "DONE interrupt should be fired after all bytes have been written",
            dataPtr != endPtr);
        while (dataPtr != endPtr) {
            statusReg = READ_REGISTER_NOFENCE_ULONG(&registersPtr->Status);
            if (!(statusReg & BCM_I2C_REG_STATUS_TXD)) {
                break;
            }

            WRITE_REGISTER_NOFENCE_ULONG(&registersPtr->DataFIFO, *dataPtr++);
        }

        writeContextPtr->CurrentWriteBufferPtr = dataPtr;
        return TRUE;
    }
    case TRANSFER_STATE::RECEIVING:
    {
        BCM_I2C_INTERRUPT_CONTEXT::READ_CONTEXT* readContextPtr =
            &interruptContextPtr->ReadContext;

        BYTE* dataPtr = readContextPtr->CurrentReadBufferPtr;
        const BYTE* const endPtr = readContextPtr->EndPtr;
        while (dataPtr != endPtr) {
            statusReg = READ_REGISTER_NOFENCE_ULONG(&registersPtr->Status);
            if (!(statusReg & BCM_I2C_REG_STATUS_RXD)) {
                break;
            }

            *dataPtr++ = static_cast<BYTE>(
                READ_REGISTER_NOFENCE_ULONG(&registersPtr->DataFIFO));
        }

        readContextPtr->CurrentReadBufferPtr = dataPtr;
        return TRUE;
    }
    case TRANSFER_STATE::SENDING_SEQUENCE:
    {
        BCM_I2C_INTERRUPT_CONTEXT::SEQUENCE_CONTEXT* sequenceContextPtr =
            &interruptContextPtr->SequenceContext;

        NT_ASSERTMSG(
            "CurrentWriteMdl can only be NULL after transitioning to RECEIVING_SEQUENCE state",
            sequenceContextPtr->CurrentWriteMdl);

        for (;;) {
            const PMDL currentWriteMdl = sequenceContextPtr->CurrentWriteMdl;
            NT_ASSERT(sequenceContextPtr->CurrentWriteMdlOffset <=
                      MmGetMdlByteCount(currentWriteMdl));
            const ULONG currentMdlBytesRemaining =
                MmGetMdlByteCount(currentWriteMdl) -
                sequenceContextPtr->CurrentWriteMdlOffset;

            // if this is the last MDL, write all but the last byte
            ULONG currentMdlBytesToWrite = currentWriteMdl->Next ?
                currentMdlBytesRemaining : (currentMdlBytesRemaining - 1);
            NT_ASSERT(
                currentWriteMdl->MdlFlags &
                (MDL_MAPPED_TO_SYSTEM_VA | MDL_SOURCE_IS_NONPAGED_POOL));
            ULONG bytesWritten = WriteFifo(
                registersPtr,
                static_cast<const BYTE*>(currentWriteMdl->MappedSystemVa) +
                    sequenceContextPtr->CurrentWriteMdlOffset,
                currentMdlBytesToWrite);
            sequenceContextPtr->BytesWritten += bytesWritten;
            sequenceContextPtr->CurrentWriteMdlOffset += bytesWritten;

            if (bytesWritten != currentMdlBytesToWrite) {
                BSC_LOG_TRACE("More bytes exist in current MDL, remaining in SENDING_SEQUENCE state.");
                NT_ASSERT(
                    sequenceContextPtr->CurrentWriteMdlOffset <
                    MmGetMdlByteCount(currentWriteMdl));
                return TRUE;
            }

            // when there is exactly one byte left to write, program the read
            if ((sequenceContextPtr->BytesWritten + 1) ==
                 sequenceContextPtr->BytesToWrite) {

                NT_ASSERT(
                    (sequenceContextPtr->CurrentWriteMdlOffset + 1) ==
                    MmGetMdlByteCount(currentWriteMdl));
                NT_ASSERT(!currentWriteMdl->Next);
                break;
            }

            BSC_LOG_TRACE(
                "Exhausted current MDL, advancing to next MDL. (currentWriteMdl = %p, currentWriteMdl->Next = %p)",
                currentWriteMdl,
                currentWriteMdl->Next);
            NT_ASSERT(
                sequenceContextPtr->CurrentWriteMdlOffset ==
                MmGetMdlByteCount(currentWriteMdl));
            NT_ASSERT(currentWriteMdl->Next);
            sequenceContextPtr->CurrentWriteMdl = currentWriteMdl->Next;
            sequenceContextPtr->CurrentWriteMdlOffset = 0;
        }

        NT_ASSERTMSG(
            "There should be exactly one more byte to write",
            (sequenceContextPtr->BytesWritten + 1) ==
             sequenceContextPtr->BytesToWrite);

        BSC_LOG_TRACE("1 byte left to write, checking TXW.");

        //
        // If TXW is not asserted, do not program the read.
        // Programming the read before TXW is asserted messes up the
        // controller's state machine.
        //
        statusReg = READ_REGISTER_NOFENCE_ULONG(&registersPtr->Status);
        if (!(statusReg & BCM_I2C_REG_STATUS_TXW)) {
            BSC_LOG_TRACE("TXW is NOT asserted, meaining the FIFO is too full. Waiting for next interrupt.");
            return TRUE;
        }

        BSC_LOG_TRACE(
            "TXW is asserted, programming the Read. (statusReg = 0x%lx)",
            statusReg);

        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->DataLength,
            sequenceContextPtr->BytesToRead);

        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->Control,
            BCM_I2C_REG_CONTROL_I2CEN |
            BCM_I2C_REG_CONTROL_ST |
            BCM_I2C_REG_CONTROL_INTR |
            BCM_I2C_REG_CONTROL_INTD |
            BCM_I2C_REG_CONTROL_READ);

        // write the last byte
        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->DataFIFO,
            *(static_cast<const BYTE*>(
                sequenceContextPtr->CurrentWriteMdl->MappedSystemVa) +
            sequenceContextPtr->CurrentWriteMdlOffset));

        ++sequenceContextPtr->BytesWritten;
        ++sequenceContextPtr->CurrentWriteMdlOffset;

        NT_ASSERT(
            sequenceContextPtr->BytesWritten ==
            sequenceContextPtr->BytesToWrite);
        NT_ASSERT(
            sequenceContextPtr->CurrentWriteMdlOffset ==
            MmGetMdlByteCount(sequenceContextPtr->CurrentWriteMdl));

        BSC_LOG_TRACE("Transitioning to ReceivingSequence state");
        NT_ASSERT(!sequenceContextPtr->CurrentWriteMdl->Next);
        sequenceContextPtr->CurrentWriteMdl = nullptr;
        interruptContextPtr->State = TRANSFER_STATE::RECEIVING_SEQUENCE;
        return TRUE;
    }
    case TRANSFER_STATE::RECEIVING_SEQUENCE:
    {
        BCM_I2C_INTERRUPT_CONTEXT::SEQUENCE_CONTEXT* sequenceContextPtr =
            &interruptContextPtr->SequenceContext;

        NT_ASSERT(
            !sequenceContextPtr->CurrentWriteMdl &&
            sequenceContextPtr->CurrentReadMdl);

        do {
            ULONG bytesRead = ReadFifoMdl(
                registersPtr,
                sequenceContextPtr->CurrentReadMdl,
                sequenceContextPtr->CurrentReadMdlOffset);
            sequenceContextPtr->BytesRead += bytesRead;
            sequenceContextPtr->CurrentReadMdlOffset += bytesRead;

            if (sequenceContextPtr->CurrentReadMdlOffset !=
                MmGetMdlByteCount(sequenceContextPtr->CurrentReadMdl)) {

                BSC_LOG_TRACE("More bytes exist in current MDL, remaining in RECEIVING_SEQUENCE state");
                return TRUE;
            }

            BSC_LOG_TRACE(
                "Read all bytes in current MDL, advancing to next MDL. (interruptContextPtr->CurrentReadMdl = %p, interruptContextPtr->CurrentReadMdl->Next = %p)",
                sequenceContextPtr->CurrentReadMdl,
                sequenceContextPtr->CurrentReadMdl->Next);

            NT_ASSERT(sequenceContextPtr->CurrentReadMdlOffset ==
                      MmGetMdlByteCount(sequenceContextPtr->CurrentReadMdl));
            sequenceContextPtr->CurrentReadMdl =
                sequenceContextPtr->CurrentReadMdl->Next;
            sequenceContextPtr->CurrentReadMdlOffset = 0;
        } while (sequenceContextPtr->CurrentReadMdl);

        BSC_LOG_TRACE("All bytes were received, waiting for DONE interrupt.");
        return TRUE;
    }
    default:
        NT_ASSERT(!"Invalid TRANSFER_STATE");
        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->Control,
            BCM_I2C_REG_CONTROL_I2CEN |
            BCM_I2C_REG_CONTROL_CLEAR);
        WRITE_REGISTER_NOFENCE_ULONG(
            &registersPtr->Status,
            BCM_I2C_REG_STATUS_ERR |
            BCM_I2C_REG_STATUS_CLKT |
            BCM_I2C_REG_STATUS_DONE);
       return TRUE;
    }
}

_Use_decl_annotations_
BOOLEAN OnInterruptIsr (WDFINTERRUPT WdfInterrupt, ULONG /*MessageID*/)
{
#ifdef LOG_ISR_TIME
    LARGE_INTEGER startQpc = KeQueryPerformanceCounter(nullptr);
#endif // LOG_ISR_TIME

    BOOLEAN claimed = HandleInterrupt(WdfInterrupt);

#ifdef LOG_ISR_TIME
    if (claimed) {
        LARGE_INTEGER qpcFrequency;
        LARGE_INTEGER stopQpc = KeQueryPerformanceCounter(&qpcFrequency);
        BSC_LOG_INFORMATION(
            "ISR Time = %lu microseconds",
            ULONG(1000000LL * (stopQpc.QuadPart - startQpc.QuadPart) /
                  qpcFrequency.QuadPart));
    }
#endif // LOG_ISR_TIME

    return claimed;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS ProcessRequestCompletion (
    BCM_I2C_INTERRUPT_CONTEXT* InterruptContextPtr,
    _Out_ ULONG* RequestInformationPtr
    )
{
    BCM_I2C_ASSERT_MAX_IRQL(DISPATCH_LEVEL);

    BCM_I2C_REGISTERS* registersPtr = InterruptContextPtr->RegistersPtr;
    ULONG capturedStatus = InterruptContextPtr->CapturedStatus;

    switch (InterruptContextPtr->State) {
    case TRANSFER_STATE::SENDING: __fallthrough;
    case TRANSFER_STATE::SENDING_SEQUENCE:
    {
        ULONG bytesToWrite =
            (InterruptContextPtr->State == TRANSFER_STATE::SENDING) ?
            (InterruptContextPtr->WriteContext.EndPtr -
             InterruptContextPtr->WriteContext.WriteBufferPtr) :
            InterruptContextPtr->SequenceContext.BytesToWrite;

        if (InterruptContextPtr->CapturedDataLength > bytesToWrite) {
            BSC_LOG_ERROR(
                "Controller reported more bytes remaining than we programmed into the DataLength register. (InterruptContextPtr->CapturedDataLength = %lu, bytesToWrite = %lu)",
                InterruptContextPtr->CapturedDataLength,
                bytesToWrite);
            *RequestInformationPtr = 0;
            return STATUS_INTERNAL_ERROR;
        }

        ULONG bytesSent =
            bytesToWrite - InterruptContextPtr->CapturedDataLength;

        // CLKT is checked before ERR because ERR is also set when CLKT is set
        if (capturedStatus & BCM_I2C_REG_STATUS_CLKT) {
            BSC_LOG_ERROR(
                "Clock stretch timeout bit of status register is set, completing request with STATUS_IO_TIMEOUT. (statusReg = 0x%lx)",
                capturedStatus);
            *RequestInformationPtr = 0;
            return STATUS_IO_TIMEOUT;
        } else if (capturedStatus & BCM_I2C_REG_STATUS_ERR) {
            if (bytesSent == 0) {
                BSC_LOG_ERROR(
                    "Error bit of status register is set and no bytes were transferred, completing request with STATUS_NO_SUCH_DEVICE. (statusReg = 0x%lx)",
                    capturedStatus);
                *RequestInformationPtr = 0;
                return STATUS_NO_SUCH_DEVICE;
            } else {
                BSC_LOG_ERROR("The slave NACKed the trasnfer before all bytes were sent - partial transfer.");
                *RequestInformationPtr = bytesSent - 1;
                NT_ASSERT(*RequestInformationPtr < bytesToWrite);
                return STATUS_SUCCESS;
            }
        } else if (capturedStatus & BCM_I2C_REG_STATUS_DONE) {
            if (bytesSent == bytesToWrite) {
                *RequestInformationPtr = bytesSent;
                return STATUS_SUCCESS;
            } else {
                BSC_LOG_ERROR(
                    "All bytes should have been written if none of the error flags are set. (bytesSent = %lu, InterruptContextPtr->BytesToWrite = %lu)",
                    bytesSent,
                    bytesToWrite);
                *RequestInformationPtr = 0;
                return STATUS_INTERNAL_ERROR;
            }
        }

        BSC_LOG_ERROR(
            "None of the expected status bits were set - unknown state. (capturedStatus = 0x%lx)",
            capturedStatus);

        *RequestInformationPtr = 0;
        return STATUS_INTERNAL_ERROR;
    }
    case TRANSFER_STATE::RECEIVING:
    {
        BCM_I2C_INTERRUPT_CONTEXT::READ_CONTEXT* readContextPtr =
            &InterruptContextPtr->ReadContext;

        if (capturedStatus & BCM_I2C_REG_STATUS_CLKT) {
            BSC_LOG_ERROR("CLKT bit was set - clock stretch timeout occurred.");
            *RequestInformationPtr = 0;
            return STATUS_IO_TIMEOUT;
        } else if (capturedStatus & BCM_I2C_REG_STATUS_ERR) {
            //
            // It is not possible for a slave device to NAK a read transfer
            // part way through. ERR bit always means the slave address
            // was not acknowledged.
            //
            BSC_LOG_ERROR("ERR bit was set - completing request with STATUS_NO_SUCH_DEVICE.");
            *RequestInformationPtr = 0;
            return STATUS_NO_SUCH_DEVICE;
        }

        readContextPtr->CurrentReadBufferPtr += ReadFifo(
            registersPtr,
            readContextPtr->CurrentReadBufferPtr,
            readContextPtr->EndPtr - readContextPtr->CurrentReadBufferPtr);

        *RequestInformationPtr = readContextPtr->CurrentReadBufferPtr -
            readContextPtr->ReadBufferPtr;
        return STATUS_SUCCESS;
    }
    case TRANSFER_STATE::RECEIVING_SEQUENCE:
    {
        BCM_I2C_INTERRUPT_CONTEXT::SEQUENCE_CONTEXT* sequenceContextPtr =
            &InterruptContextPtr->SequenceContext;

        NT_ASSERTMSG(
            "We should only reach the RECEIVING_SEQUENCE state if the entire write buffer was queued",
            !sequenceContextPtr->CurrentWriteMdl &&
            (sequenceContextPtr->BytesWritten ==
             sequenceContextPtr->BytesToWrite));

        //
        // Due to the requirement that the read must be queued before the write
        // completes, the write FIFO could still have data in it that was never
        // sent, and reading from the FIFO would give us back our unsent write
        // buffer. We must check for this condition before reading from the
        // FIFO. If one of the error bits is set, the transfer most likely
        // failed during the write portion.
        //
        if (capturedStatus & BCM_I2C_REG_STATUS_CLKT) {
            BSC_LOG_ERROR("CLKT was set - completing request with STATUS_IO_TIMEOUT.");
            *RequestInformationPtr = 0;
            return STATUS_IO_TIMEOUT;
        } else if (capturedStatus & BCM_I2C_REG_STATUS_ERR) {
            if (!(capturedStatus & BCM_I2C_REG_STATUS_DONE) ||
                 (InterruptContextPtr->CapturedDataLength == 0)) {

                //
                // It is not possible to tell exactly how many bytes were
                // transferred in this case because DataLength was
                // necessarily clobberred when the read was queued.
                // Report a partial transfer of 0 bytes.
                //
                BSC_LOG_ERROR("The write was NAKed before all bytes could be transmitted - partial transfer.");
                *RequestInformationPtr = 0;
                return STATUS_SUCCESS;
            } else {
                BSC_LOG_ERROR("The slave address was not acknowledged.");
                *RequestInformationPtr = 0;
                return STATUS_NO_SUCH_DEVICE;
            }
        }

        while (sequenceContextPtr->CurrentReadMdl) {
            ULONG bytesRead = ReadFifoMdl(
                registersPtr,
                sequenceContextPtr->CurrentReadMdl,
                sequenceContextPtr->CurrentReadMdlOffset);
            sequenceContextPtr->BytesRead += bytesRead;
            sequenceContextPtr->CurrentReadMdlOffset += bytesRead;

            // Only advance to next MDL if current MDL is exhausted.
            // If the current MDL is not exhausted, it means there was a
            // partial transfer.
            if (sequenceContextPtr->CurrentReadMdlOffset !=
                MmGetMdlByteCount(sequenceContextPtr->CurrentReadMdl)) {

                break;
            }

            NT_ASSERT(bytesRead != 0);
            sequenceContextPtr->CurrentReadMdl =
                sequenceContextPtr->CurrentReadMdl->Next;
            sequenceContextPtr->CurrentReadMdlOffset = 0;
        }

        *RequestInformationPtr = sequenceContextPtr->BytesWritten +
            sequenceContextPtr->BytesRead;
        return STATUS_SUCCESS;
    }
    default:
        // unrecognized state
        NT_ASSERT(!"Invalid TRANSFER_STATE value");
        *RequestInformationPtr = 0;
        return STATUS_INTERNAL_ERROR;
    }
}

_Use_decl_annotations_
VOID OnInterruptDpc (WDFINTERRUPT WdfInterrupt, WDFOBJECT /*WdfDevice*/)
{
    NTSTATUS status;
    BCM_I2C_INTERRUPT_CONTEXT* interruptContextPtr =
        GetInterruptContext(WdfInterrupt);
    BCM_I2C_REGISTERS* registersPtr = interruptContextPtr->RegistersPtr;

    BSC_LOG_TRACE(
        "DPC occurred. (InterruptContextPtr->State = %d)",
        int(interruptContextPtr->State));

    NT_ASSERTMSG(
        "Interrupts should be disabled when the DPC is invoked",
        !(READ_REGISTER_NOFENCE_ULONG(&registersPtr->Control) &
         (BCM_I2C_REG_CONTROL_INTD |
          BCM_I2C_REG_CONTROL_INTT |
          BCM_I2C_REG_CONTROL_INTR)));

    // The cancellation callback sets SpbRequest to NULL on cancellation
    SPBREQUEST spbRequest = static_cast<SPBREQUEST>(InterlockedExchangePointer(
        reinterpret_cast<PVOID *>(&interruptContextPtr->SpbRequest),
        nullptr));

    if (!spbRequest) {
        BSC_LOG_INFORMATION("DPC invoked for cancelled request.");
        return;
    }

    {
        status = WdfRequestUnmarkCancelable(spbRequest);
        switch (status) {
        case STATUS_SUCCESS:
            break;
        case STATUS_CANCELLED:
            BSC_LOG_INFORMATION(
                "Cancellation was requested but we beat the cancellation routine to the request. Canceling the request. (spbRequest = %p)",
                spbRequest);
            SpbRequestComplete(spbRequest, STATUS_CANCELLED);
            return;
        case STATUS_INVALID_PARAMETER:
        case STATUS_INVALID_DEVICE_REQUEST:
        default:
            BSC_LOG_ERROR(
                "Failed to unmark request cancelable. (spbRequest = %p, status = %!STATUS!)",
                spbRequest,
                status);
            SpbRequestComplete(spbRequest, STATUS_INTERNAL_ERROR);
            return;
        }
    }

    ULONG information;
    status = ProcessRequestCompletion(interruptContextPtr, &information);

    //
    // Always clear hardware FIFOs before completing request to aid in bus
    // error recovery and to prevent data leakage.
    //
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Control,
        BCM_I2C_REG_CONTROL_I2CEN | BCM_I2C_REG_CONTROL_CLEAR);

    BSC_LOG_INFORMATION(
        "Completing request. (spbRequest = %p, information = %lu, status = %!STATUS!)",
        spbRequest,
        information,
        status);

    WdfRequestSetInformation(spbRequest, information);
    SpbRequestComplete(spbRequest, status);
}

BCM_I2C_NONPAGED_SEGMENT_END; //===============================================
BCM_I2C_PAGED_SEGMENT_BEGIN; //================================================

_Use_decl_annotations_
NTSTATUS OnPrepareHardware (
    WDFDEVICE WdfDevice,
    WDFCMRESLIST /*ResourcesRaw*/,
    WDFCMRESLIST ResourcesTranslated
    )
{
    PAGED_CODE();
    BCM_I2C_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    const CM_PARTIAL_RESOURCE_DESCRIPTOR* memResourcePtr = nullptr;
    ULONG interruptResourceCount = 0;

    // Look for single memory resource and single interrupt resource
    const ULONG resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);
    for (ULONG i = 0; i < resourceCount; ++i) {
        const PCM_PARTIAL_RESOURCE_DESCRIPTOR resourcePtr =
            WdfCmResourceListGetDescriptor(ResourcesTranslated, i);

        switch (resourcePtr->Type) {
        case CmResourceTypeMemory:
            // take the first memory resource found
            if (!memResourcePtr) {
                memResourcePtr = resourcePtr;
            }
            break;
        case CmResourceTypeInterrupt:
            ++interruptResourceCount;
            break;
        }
    }

    if (!memResourcePtr ||
        (memResourcePtr->u.Memory.Length < sizeof(BCM_I2C_REGISTERS)) ||
        (interruptResourceCount == 0))
    {
        BSC_LOG_ERROR(
            "Did not receive required memory resource and interrupt resource. (ResourcesTranslated = %p, memResourcePtr = %p, interruptResourceCount = %lu)",
            ResourcesTranslated,
            memResourcePtr,
            interruptResourceCount);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    NT_ASSERT(memResourcePtr->Type == CmResourceTypeMemory);
    BCM_I2C_REGISTERS* registersPtr = static_cast<BCM_I2C_REGISTERS*>(
        MmMapIoSpaceEx(
            memResourcePtr->u.Memory.Start,
            memResourcePtr->u.Memory.Length,
            PAGE_READWRITE | PAGE_NOCACHE));

    if (!registersPtr) {
        BSC_LOG_LOW_MEMORY(
            "Failed to map registers - returning STATUS_INSUFFICIENT_RESOURCES. (memResourcePtr->u.Memory.Start = %I64u, memResourcePtr->u.Memory.Length = %lu)",
            memResourcePtr->u.Memory.Start.QuadPart,
            memResourcePtr->u.Memory.Length);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Disable and acknowledge interrupts before entering D0 state to prevent
    // spurious interrupts.
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Control,
        BCM_I2C_REG_CONTROL_CLEAR);
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Status,
        BCM_I2C_REG_STATUS_DONE |
        BCM_I2C_REG_STATUS_ERR |
        BCM_I2C_REG_STATUS_CLKT);

    BCM_I2C_DEVICE_CONTEXT* devicePtr = GetDeviceContext(WdfDevice);
    devicePtr->RegistersPtr = registersPtr;
    devicePtr->RegistersPhysicalAddress = memResourcePtr->u.Memory.Start;
    devicePtr->RegistersLength = memResourcePtr->u.Memory.Length;
    devicePtr->InterruptContextPtr->RegistersPtr = registersPtr;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS OnReleaseHardware (
    WDFDEVICE WdfDevice,
    WDFCMRESLIST /*ResourcesTranslated*/
    )
{
    PAGED_CODE();
    BCM_I2C_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    BCM_I2C_DEVICE_CONTEXT* devicePtr = GetDeviceContext(WdfDevice);
    if (devicePtr->RegistersPtr) {
        MmUnmapIoSpace(devicePtr->RegistersPtr, devicePtr->RegistersLength);

        devicePtr->RegistersPtr = nullptr;
        devicePtr->RegistersLength = 0;
        devicePtr->RegistersPhysicalAddress = PHYSICAL_ADDRESS();
        devicePtr->InterruptContextPtr->RegistersPtr = nullptr;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS OnD0Entry (
    WDFDEVICE WdfDevice,
    WDF_POWER_DEVICE_STATE /*PreviousState*/
    )
{
    PAGED_CODE();
    BCM_I2C_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    BCM_I2C_DEVICE_CONTEXT* devicePtr = GetDeviceContext(WdfDevice);
    BCM_I2C_REGISTERS* registersPtr = devicePtr->RegistersPtr;

    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Control,
        BCM_I2C_REG_CONTROL_I2CEN |
        BCM_I2C_REG_CONTROL_CLEAR);
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Status,
        BCM_I2C_REG_STATUS_DONE |
        BCM_I2C_REG_STATUS_ERR |
        BCM_I2C_REG_STATUS_CLKT);
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->ClockDivider,
        BCM_I2C_REG_CDIV_DEFAULT);
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->DataDelay,
        BCM_I2C_REG_DEL_DEFAULT);
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->ClockStretchTimeout,
        BCM_I2C_REG_CLKT_TOUT);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS OnD0Exit (
    WDFDEVICE WdfDevice,
    WDF_POWER_DEVICE_STATE /*PreviousState*/
    )
{
    PAGED_CODE();
    BCM_I2C_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    BCM_I2C_DEVICE_CONTEXT* devicePtr = GetDeviceContext(WdfDevice);
    BCM_I2C_REGISTERS* registersPtr = devicePtr->RegistersPtr;

    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Control,
        BCM_I2C_REG_CONTROL_CLEAR);
    WRITE_REGISTER_NOFENCE_ULONG(
        &registersPtr->Status,
        BCM_I2C_REG_STATUS_DONE |
        BCM_I2C_REG_STATUS_ERR |
        BCM_I2C_REG_STATUS_CLKT);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS OnTargetConnect (WDFDEVICE /*WdfDevice*/, SPBTARGET SpbTarget)
{
    PAGED_CODE();
    BCM_I2C_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    //
    // Get ACPI descriptor
    //
    const PNP_I2C_SERIAL_BUS_DESCRIPTOR* i2cDescriptorPtr;
    {
        SPB_CONNECTION_PARAMETERS params;
        SPB_CONNECTION_PARAMETERS_INIT(&params);

        SpbTargetGetConnectionParameters(SpbTarget, &params);

        const auto rhBufferPtr =
            static_cast<RH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER*>(
                params.ConnectionParameters);
        if (rhBufferPtr->PropertiesLength < sizeof(*i2cDescriptorPtr)) {
            BSC_LOG_ERROR(
                "Connection properties is too small. (rhBufferPtr->PropertiesLength = %lu, sizeof(*i2cDescriptorPtr) = %lu)",
                rhBufferPtr->PropertiesLength,
                sizeof(*i2cDescriptorPtr));
            return STATUS_INVALID_PARAMETER;
        }

        i2cDescriptorPtr = reinterpret_cast<PNP_I2C_SERIAL_BUS_DESCRIPTOR*>(
            rhBufferPtr->ConnectionProperties);

        if (i2cDescriptorPtr->SerialBusType != PNP_SERIAL_BUS_TYPE_I2C) {
            BSC_LOG_ERROR(
                "ACPI Connnection descriptor is not an I2C connection descriptor. (i2cDescriptorPtr->SerialBusType = 0x%lx, PNP_SERIAL_BUS_TYPE_I2C = 0x%lx)",
                i2cDescriptorPtr->SerialBusType,
                PNP_SERIAL_BUS_TYPE_I2C);
            return STATUS_INVALID_PARAMETER;
        }
    }

    if (i2cDescriptorPtr->GeneralFlags & I2C_SLV_BIT) {
        BSC_LOG_ERROR(
            "Slave mode is not supported. Only ControllerInitiated mode is supported. (i2cDescriptorPtr->GeneralFlags = 0x%lx)",
            i2cDescriptorPtr->GeneralFlags);
        return STATUS_NOT_SUPPORTED;
    }

    if (i2cDescriptorPtr->TypeSpecificFlags &
        I2C_SERIAL_BUS_SPECIFIC_FLAG_10BIT_ADDRESS) {

        BSC_LOG_ERROR(
            "10-bit addressing is not supported. (i2cDescriptorPtr->TypeSpecificFlags = 0x%lx)",
            i2cDescriptorPtr->TypeSpecificFlags);
        return STATUS_NOT_SUPPORTED;
    }

    if (i2cDescriptorPtr->Address > I2C_MAX_ADDRESS) {
        BSC_LOG_ERROR(
            "Slave address is out of range. (i2cDescriptorPtr->Address = 0x%lx, I2C_MAX_ADDRESS = 0x%lx)",
            i2cDescriptorPtr->Address,
            I2C_MAX_ADDRESS);
        return STATUS_INVALID_PARAMETER;
    }

    if ((i2cDescriptorPtr->ConnectionSpeed < BCM_I2C_MIN_CONNECTION_SPEED) ||
        (i2cDescriptorPtr->ConnectionSpeed > BCM_I2C_MAX_CONNECTION_SPEED)) {

        BSC_LOG_ERROR(
            "ConnectionSpeed is out of supported range. (i2cDescriptorPtr->ConnectionSpeed = %lu, BCM_I2C_MIN_CONNECTION_SPEED = %lu, BCM_I2C_MAX_CONNECTION_SPEED = %lu)",
            i2cDescriptorPtr->ConnectionSpeed,
            BCM_I2C_MIN_CONNECTION_SPEED,
            BCM_I2C_MAX_CONNECTION_SPEED);
        return STATUS_NOT_SUPPORTED;
    }

    BCM_I2C_TARGET_CONTEXT* targetPtr = GetTargetContext(SpbTarget);
    targetPtr->Address = i2cDescriptorPtr->Address;
    targetPtr->ConnectionSpeed = i2cDescriptorPtr->ConnectionSpeed;

    BSC_LOG_TRACE(
        "Connected to SPBTARGET. (SpbTarget = %p, targetPtr->Address = 0x%lx, targetPtr->ConnectionSpeed = %lu)",
        SpbTarget,
        targetPtr->Address,
        targetPtr->ConnectionSpeed);

    return STATUS_SUCCESS;
}

BCM_I2C_PAGED_SEGMENT_END; //==================================================
