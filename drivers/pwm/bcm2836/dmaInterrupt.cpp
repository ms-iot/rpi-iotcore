/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:

    This file contains functions for DMA interrupt processing.

--*/

#include "driver.h"
#include "dmainterrupt.tmh"

//
// Define ISRDPC_DEBUG to enable additional ETW debug output in ISR and DPC functions.
//

//#define ISRDPC_DEBUG

#pragma code_seg()
_Use_decl_annotations_
VOID
ClearDmaErrorAndRequestRestart
(
    PDEVICE_CONTEXT DeviceContext
)
/*++

Routine Description:

    Clear all DMA and PWM error flags and requests a restart of the DMA.

Arguments:

    DeviceContext - device context

Return Value:

    None

--*/
{
    //
    // Clear error bits in DMA debug register and error bits in PWM status register.
    //

    ULONG pwmStatus = READ_REGISTER_ULONG(&DeviceContext->pwmRegs->STA);
    ULONG dmaDebug = READ_REGISTER_ULONG(&DeviceContext->dmaChannelRegs->DEBUG);
    WRITE_REGISTER_ULONG(&DeviceContext->pwmRegs->STA, pwmStatus & ~(PWM_STA_BERR | PWM_STA_GAPO1 | PWM_STA_GAPO2 | PWM_STA_RERR1 | PWM_STA_WERR1));
    WRITE_REGISTER_ULONG(&DeviceContext->dmaChannelRegs->DEBUG, (DMA_DEBUG_FIFO_ERROR | DMA_DEBUG_READ_ERROR | DMA_DEBUG_READ_LAST_NOT_SET_ERROR));

    //
    // Request DMA restart.
    //

    InterlockedExchange((PLONG)&DeviceContext->dmaRestartRequired, (LONG)TRUE);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "PWM STA: 0x%08x, DMA DEBUG: 0x%08x", pwmStatus, dmaDebug);
}

#pragma code_seg()
_Use_decl_annotations_
ULONG
GetNumberOfProcessedPackets
(
    ULONG CompletedPacket,
    ULONG LastKnownCompletedPacket,
    ULONG NumPackets
)
/*++

Routine Description:

    Calculates the nubmer of packets processed by DMA since the last interrupt.

Arguments:

    CompletedPacket - index of last packet processed by DMA
    
    LastKnownCompletedPacket - index of the packet processed before CompletedPacket
    
    NumPackets - number of packets in DMA buffer
    
Return Value:

    number of packets transmitted since LastKnownCompletedPacket

--*/
{
        ULONG processedPackets = 0; 
        if (LastKnownCompletedPacket == NO_LAST_COMPLETED_PACKET) 
        { 
            //
            // We failed at the first call.
            //

            processedPackets = CompletedPacket + 1;
        }
        else
        {
            //
            // Handle buffer wrap around.
            //

            if (CompletedPacket > LastKnownCompletedPacket)
            {
                processedPackets = CompletedPacket - LastKnownCompletedPacket;
            }
            else
            {
                processedPackets = CompletedPacket + NumPackets - LastKnownCompletedPacket;
            }
        }
    NT_ASSERT(processedPackets != 0);
    NT_ASSERT(processedPackets <= NumPackets);

    return processedPackets;
}

#pragma code_seg()
_Use_decl_annotations_
VOID
HandleUnderflow(
    PDEVICE_CONTEXT DeviceContext
    )
    /*++

    Routine Description:

        Handles underflow conditions for ISR and DPC.

    Arguments:

        DeviceContext - device context

    Return Value:

        None

    --*/
{
    ULONG conblk_ad = READ_REGISTER_ULONG(&DeviceContext->dmaChannelRegs->CONBLK_AD);
    if (conblk_ad == 0)
    {
        // 
        // Since CONBLK_AD is zero at this point, we could not detect the last completed packet based on it.
        // SOURCE_AD is still valid and points to the data area of the next packet. The initial SOURCE_AD value is
        // set by the first control block of the packet.
        // We use the SOURCE_AD register to identify the packet we have just completed.
        //

        ULONG source_ad = READ_REGISTER_ULONG(&DeviceContext->dmaChannelRegs->SOURCE_AD);
        NT_ASSERT(source_ad);
        ULONG lastKnownCompletedPacket = DeviceContext->dmaLastKnownCompletedPacket;
        ULONG currentPacket = 0;

        for (; currentPacket < DeviceContext->dmaNumPackets; currentPacket++)
        {
            //
            // If the SOURCE_AD is larger than the init value of the current packets init value for this register,
            // check next packet.
            //

            ULONG currentPacketSourceAddressInitValue = SOURCE_AD_INIT_VALUE_OF_PACKET(currentPacket, DeviceContext->dmaCb);
            if (source_ad > currentPacketSourceAddressInitValue)
            {
                continue;
            }
            break;
        }

        ULONG completedPacket = (currentPacket ? currentPacket - 1 : DeviceContext->dmaNumPackets - 1);
        NT_ASSERT(completedPacket < DeviceContext->dmaNumPackets);

        //
        // Compute the number of packets processed. It might be that we miss an interrupt and there were
        // two or more packets completed, since the last call of the ISR.
        //

        ULONG processedPackets = GetNumberOfProcessedPackets(completedPacket, lastKnownCompletedPacket, DeviceContext->dmaNumPackets);

        //
        // Update counters.
        //

        ULONG lastPacketsInUse = InterlockedExchange((LONG*)&DeviceContext->dmaPacketsInUse, 0);
        InterlockedAdd((LONG*)&DeviceContext->dmaPacketsProcessed, processedPackets);
        DeviceContext->dmaUnderflowErrorCount++;

        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IO, "DMA underflow condition detected (%d), Packets in use: %d",
            DeviceContext->dmaUnderflowErrorCount, DeviceContext->dmaPacketsInUse);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "DMA Notification count: %d, Last known completed packet: %d, Packets processed: %d",
            DeviceContext->dmaAudioNotifcationCount, DeviceContext->dmaLastKnownCompletedPacket, (ULONG)DeviceContext->dmaPacketsProcessed);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "Current packet: %d, Completed packet: %d, Currently processed: %d Last in-use count: %d",
            currentPacket, completedPacket, processedPackets, lastPacketsInUse);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IO, "DMA DEBUG: 0x%08x, DMA SOURCE_AD: 0x%08x, PWM STA: 0x%08x",
            READ_REGISTER_ULONG(&DeviceContext->dmaChannelRegs->DEBUG), READ_REGISTER_ULONG(&DeviceContext->dmaChannelRegs->SOURCE_AD), READ_REGISTER_ULONG(&DeviceContext->pwmRegs->STA));

        //
        // Clear error bits and request restart.
        //

        ClearDmaErrorAndRequestRestart(DeviceContext);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "Underflow detected - clear error bits and request restart");
    }
    else
    {
        //
        // We should hit this path only for a pause, not for an underflow. We do nothing and just fall through.
        //

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "DMA pause detected (0x%08x)", conblk_ad);
    }
}

#pragma code_seg()
_Use_decl_annotations_
BOOLEAN 
DmaIsr(
    WDFINTERRUPT Interrupt,
    ULONG MessageId
)
/*++

Routine Description:

    This function is the ISR for the DMA complete interrupt.

Arguments:

    Interrupt - a pointer to the WDFINTERRUPT object
    MessageId - the interrupt message id

Return Value:

    TRUE if the interrupt was processed

--*/
{
    UNREFERENCED_PARAMETER(MessageId);

    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;

    device = WdfInterruptGetDevice(Interrupt);
    deviceContext = GetContext(device);

    //
    // Read DMA status.
    //

    ULONG conblk_ad = READ_REGISTER_ULONG(&deviceContext->dmaChannelRegs->CONBLK_AD);
    ULONG cs = READ_REGISTER_ULONG(&deviceContext->dmaChannelRegs->CS);

    //
    // Mask bits which could not be read.
    //

    cs &= ~(DMA_CS_RESET | DMA_CS_ABORT);

    //
    // All W1C flags (END/INT) should be cleared, all RW bits should be unchanged (same value written back), all RO bits are ignored.
    //

    WRITE_REGISTER_ULONG(&deviceContext->dmaChannelRegs->CS, cs);

    if (cs & DMA_CS_INT)
    {
        //
        // Check for error condition.
        //

        if (cs & DMA_CS_ERROR)
        {
            //
            // Clear error bits and request restart.
            //

            ClearDmaErrorAndRequestRestart(deviceContext);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "DMA error detected - clear error bits and request restart (0x%08x)", cs);
        }
        else
        {
            //
            // Analyze interrupt root cause.
            //

            if ((cs & DMA_CS_ACTIVE) == 0)
            {
                //
                // DMA is no longer active. We hit an underflow condition. CONBLK_AD must be 0.
                //

                NT_ASSERT(conblk_ad == 0);

                //
                // Handle underflow condition.
                //

                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "DMA underflow condition detected");
                HandleUnderflow(deviceContext);
            }
            else
            {
                //
                // DMA is still active.
                //

                NT_ASSERT(conblk_ad);

                //
                // If the packet count in the buffer allows to add enough (dmaPacketToPrimePreset) packets and current packets to add to buffer without packets
                // actually transmitted by DMA (dmaPacketsToPrime) is smaller than the preset, then request more packets (dmaPacketsToPrimePreset) from the audio
                // stack.
                //

                if (deviceContext->dmaPacketsInUse < (deviceContext->dmaNumPackets - deviceContext->dmaPacketsToPrimePreset) && deviceContext->dmaPacketsToPrime < deviceContext->dmaPacketsToPrimePreset)
                {
                    InterlockedExchange((LONG*)&deviceContext->dmaPacketsToPrime, deviceContext->dmaPacketsToPrimePreset);
#ifdef ISRDPC_DEBUG
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "Only %d packets in buffer. Request buffer priming with %d packets",
                        deviceContext->dmaPacketsInUse, deviceContext->dmaPacketsToPrimePreset);
#endif
                }

                //
                // Compute the last processed packet based on the value of current CONBLK_AD value.
                // The control block currently active is already beyond the packet we have just completed.
                //

                ULONG lastKnownCompletedPacket = deviceContext->dmaLastKnownCompletedPacket;
                ULONG currentPacket = 0;

                for (; currentPacket < deviceContext->dmaNumPackets; currentPacket++)
                {
                    //
                    // If the current CONBLK_AD register value is larger than the currently inspected packets first control block address (each packet
                    // has two control blocks), check next packet.
                    //

                    ULONG currentPacketFirstCbAddress = FIRST_CB_ADDRESS_OF_PACKET(currentPacket, deviceContext->dmaCbPa.LowPart);
                    if (conblk_ad > currentPacketFirstCbAddress)
                    {
                        continue;
                    }
                    break;
                }

                ULONG completedPacket = PREVIOUS_PACKET_INDEX(currentPacket, deviceContext->dmaNumPackets);
                deviceContext->dmaLastKnownCompletedPacket = completedPacket;
                NT_ASSERT(completedPacket < deviceContext->dmaNumPackets);

                //
                // Compute the number of packets processed. It might be that we miss an interrupt and there were
                // two or more packets completed, since the last call of the ISR.
                //

                ULONG processedPackets = GetNumberOfProcessedPackets(completedPacket, lastKnownCompletedPacket, deviceContext->dmaNumPackets);

#ifdef ISRDPC_DEBUG
                {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "current: %d, completed: %d, lastknowncompleted: %d, processed: %d, inuse: %d, toprime: %d",
                        currentPacket, completedPacket, lastKnownCompletedPacket, processedPackets, deviceContext->dmaPacketsInUse, deviceContext->dmaPacketsToPrime);
                }
#endif

                //
                // Adjust in use packet count and unlink each of the processed packets.
                // If the DMA controller reads the 2nd control block of an unlinked packet,
                // it reads 0 as the NEXTCONBK, which stops the DMA and allows to identify an underflow condition.
                // Note: The dmaPacketLinkInfo resides in non cached memory, which could not be used with InterlockedExchange.
                // Since the LinkPtr points to the NEXTCONBK field of a DMA control block, the pointer points to a 32 bit
                // aligned memory location and the access is atomic by default.
                //

                ULONG packetToUnlink = completedPacket;
                for (ULONG ul = 0; ul < processedPackets; ul++)
                {
                    *((LONG*)deviceContext->dmaPacketLinkInfo[packetToUnlink].LinkPtr) = 0;
                    packetToUnlink = PREVIOUS_PACKET_INDEX(packetToUnlink, deviceContext->dmaNumPackets);
                }
                InterlockedAdd((LONG*)&deviceContext->dmaPacketsInUse, -1L * (LONG)processedPackets);
                InterlockedAdd((LONG*)&deviceContext->dmaPacketsProcessed, processedPackets);
            }
            deviceContext->dmaLastProcessedPacketTime = KeQueryPerformanceCounter(NULL);
        }

        //
        // Queue a DPC for further processing.
        //

        if (FALSE == WdfInterruptQueueDpcForIsr(deviceContext->interruptObj))
        {
            deviceContext->dmaDpcForIsrErrorCount++;
            if (deviceContext->dmaDpcForIsrErrorCount == 1)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_IO, "DpcForIsr could not be queued. This message will only show once. (%d, 0x%08x))", deviceContext->dmaDpcForIsrErrorCount, cs);
            }
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "Interrupt not from PWM DMA. Ingoring.");
        return FALSE;
    }
    return TRUE;
}

#pragma code_seg()
_Use_decl_annotations_
VOID
DmaDpc(
    WDFINTERRUPT Interrupt,
    WDFOBJECT AssociatedObject
)
/*++

Routine Description:

    This function is the DPC for the DMA complete interrupt.

Arguments:

    Interrupt - a pointer to the WDFINTERRUPT object
    AssociatedObject - pointer to the WDFOBJECT associated with the DPC

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(AssociatedObject);
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;

    device = WdfInterruptGetDevice(Interrupt);
    deviceContext = GetContext(device);

    //
    // Read DMA status.
    //

    ULONG cs = READ_REGISTER_ULONG(&deviceContext->dmaChannelRegs->CS);

    //
    // If DMA is not active and no restart pending.
    //

    if ((cs & DMA_CS_ACTIVE) == 0 && (deviceContext->dmaRestartRequired == FALSE))
    {
        //
        // Handle underflow condition. This underflow may happen while the ISR is running. In this case we are loosing the interrupt
        // and the DMA is stopped.
        //

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IO, "DMA underflow condition detected");
        HandleUnderflow(deviceContext);
    }

    //
    // Notify listeners.
    //

    deviceContext->dmaAudioNotifcationCount++;
    WdfSpinLockAcquire(deviceContext->notificationListLock);

    if (!IsListEmpty(&deviceContext->notificationList))
    {
        PLIST_ENTRY currentNotificationListEntry = deviceContext->notificationList.Flink;
        while (currentNotificationListEntry != &deviceContext->notificationList)
        {
            PNOTIFICATION_LIST_ENTRY currentNotification = CONTAINING_RECORD(currentNotificationListEntry, NOTIFICATION_LIST_ENTRY, ListEntry);
            KeSetEvent(currentNotification->NotificationEvent, 0, FALSE);
            currentNotificationListEntry = currentNotificationListEntry->Flink;
        }
    }

    WdfSpinLockRelease(deviceContext->notificationListLock);

    return;
}