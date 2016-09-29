// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//   power.c
//
// Abstract:
//
///    This module contains the code that handles the power IRPs for the serial
//    driver.
//

#include "precomp.h"
#include "power.tmh"


PCHAR DbgDevicePowerString(_In_ WDF_POWER_DEVICE_STATE Type);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGESER,SerialEvtDeviceD0Exit)
#pragma alloc_text(PAGESER,SerialSaveDeviceState)
#endif

/*++
Routine Description:

     Uses mapping of device power states to strings describing power states

Parameters Description:

     Type - device power state

Return Value:

     driver power state as string

*/
_Use_decl_annotations_
PCHAR
DbgDevicePowerString(
    WDF_POWER_DEVICE_STATE Type
    )
{

    switch (Type)
    {
    case WdfPowerDeviceInvalid:
        return "WdfPowerDeviceInvalid";
    case WdfPowerDeviceD0:
        return "WdfPowerDeviceD0";
    case WdfPowerDeviceD1:
        return "WdfPowerDeviceD1";
    case WdfPowerDeviceD2:
        return "WdfPowerDeviceD2";
    case WdfPowerDeviceD3:
        return "WdfPowerDeviceD3";
    case WdfPowerDeviceD3Final:
        return "WdfPowerDeviceD3Final";
    case WdfPowerDevicePrepareForHibernation:
        return "WdfPowerDevicePrepareForHibernation";
    case WdfPowerDeviceMaximum:
        return "WdfPowerDeviceMaximum";
    default:
        return "UnKnown Device Power State";
    }
}

/*++

Routine Description:

    EvtDeviceD0Entry event callback must perform any operations that are
    necessary before the specified device is used.  It will be called every
    time the hardware needs to be (re-)initialized.  This includes after
    IRP_MN_START_DEVICE, IRP_MN_CANCEL_STOP_DEVICE, IRP_MN_CANCEL_REMOVE_DEVICE,
    IRP_MN_SET_POWER-D0.

    This function is not marked pageable because this function is in the
    device power up path. When a function is marked pagable and the code
    section is paged out, it will generate a page fault which could impact
    the fast resume behavior because the client driver will have to wait
    until the system drivers can service this page fault.

    This function runs at PASSIVE_LEVEL, even though it is not paged.  A
    driver can optionally make this function pageable if DO_POWER_PAGABLE
    is set.  Even if DO_POWER_PAGABLE isn't set, this function still runs
    at PASSIVE_LEVEL.  In this case, though, the function absolutely must
    not do anything that will cause a page fault.

Arguments:

    Device - Handle to a framework device object.

    PreviousState - Device power state which the device was in most recently.
        If the device is being newly started, this will be
        PowerDeviceUnspecified.

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS
SerialEvtDeviceD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
    )
{
    PSERIAL_DEVICE_EXTENSION deviceExtension;
    PSERIAL_DEVICE_STATE pDevState;
    SHORT divisor;
    SERIAL_IOCTL_SYNC serSync;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER,
                "++SerialEvtDeviceD0Entry - coming from %s\r\n", 
                DbgDevicePowerString(PreviousState));

    deviceExtension = SerialGetDeviceExtension (Device);
    pDevState = &deviceExtension->DeviceState;

    // If there is a debugger conflict, avoid accessing the hardware since
    // the UART driver is only present for preventing any application/driver
    // from muxing-out the debugger.

    if (deviceExtension->DebugPortInUse) {
        ASSERT(deviceExtension->FunctionConfigConnectionId.QuadPart != 0LL);
        goto End;
    }

    // Restore the state of the UART.  First, that involves disabling
    // interrupts both via OUT2 and IER.

    WRITE_MODEM_CONTROL(deviceExtension, deviceExtension->Controller, 0);
    DISABLE_ALL_INTERRUPTS(deviceExtension, deviceExtension->Controller);

    // Set the baud rate

    SerialGetDivisorFromBaud(deviceExtension->ClockRate,
                            deviceExtension->CurrentBaud,
                            &divisor);

    serSync.Extension = deviceExtension;
    serSync.Data = (PVOID) (ULONG_PTR) divisor;

#pragma prefast(suppress: __WARNING_INFERRED_IRQ_TOO_LOW, "PFD warning that we are calling interrupt synchronize routine directly. Suppress it because interrupt is disabled above.")

    SerialSetBaud(deviceExtension->WdfInterrupt, &serSync);

    // Reset / Re-enable the FIFO's

    if (deviceExtension->FifoPresent) {

       WRITE_FIFO_CONTROL(deviceExtension, deviceExtension->Controller, (UCHAR)0);
       READ_RECEIVE_BUFFER(deviceExtension, deviceExtension->Controller);

       WRITE_FIFO_CONTROL(deviceExtension,
                            deviceExtension->Controller,
                            (UCHAR)(SERIAL_FCR_ENABLE | deviceExtension->RxFifoTrigger
                                  | SERIAL_FCR_RCVR_RESET
                                  | SERIAL_FCR_TXMT_RESET));
    } else {
       WRITE_FIFO_CONTROL(deviceExtension, deviceExtension->Controller, (UCHAR)0);
    }

    // Restore a couple more registers

    WRITE_INTERRUPT_ENABLE(deviceExtension, deviceExtension->Controller, pDevState->IER);
    WRITE_LINE_CONTROL(deviceExtension, deviceExtension->Controller, pDevState->LCR);

    // Clear out any stale interrupts

    READ_INTERRUPT_ID_REG(deviceExtension, deviceExtension->Controller);
    READ_LINE_STATUS(deviceExtension, deviceExtension->Controller);
    READ_MODEM_STATUS(deviceExtension, deviceExtension->Controller);

    // TODO:  move this code to EvtInterruptEnable.

    if (deviceExtension->DeviceState.Reopen == TRUE) {
       TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "Reopening mini Uart device\r\n");

       SetDeviceIsOpened(deviceExtension, TRUE, FALSE);

       // This enables interrupts on mini Uart device

       WRITE_MODEM_CONTROL(deviceExtension,
                            deviceExtension->Controller,
                            (UCHAR)(pDevState->MCR | SERIAL_MCR_OUT2));

       // Refire the state machine

       DISABLE_ALL_INTERRUPTS(deviceExtension, deviceExtension->Controller);
       ENABLE_ALL_INTERRUPTS(deviceExtension, deviceExtension->Controller);
    }

End:

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "--SerialEvtDeviceD0Entry\r\n");

    return STATUS_SUCCESS;
}

/*++

Routine Description:

   EvtDeviceD0Exit event callback must perform any operations that are
   necessary before the specified device is moved out of the D0 state.  If the
   driver needs to save hardware state before the device is powered down, then
   that should be done here.

   This function runs at PASSIVE_LEVEL, though it is generally not paged.  A
   driver can optionally make this function pageable if DO_POWER_PAGABLE is set.

   Even if DO_POWER_PAGABLE isn't set, this function still runs at
   PASSIVE_LEVEL.  In this case, though, the function absolutely must not do
   anything that will cause a page fault.

Arguments:

    Device - Handle to a framework device object.

    TargetState - Device power state which the device will be put in once this
        callback is complete.

Return Value:

    NTSTATUS

--*/
_Use_decl_annotations_
NTSTATUS
SerialEvtDeviceD0Exit(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
    )
{
    PSERIAL_DEVICE_EXTENSION deviceExtension;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER,
                "++SerialEvtDeviceD0Exit - moving to %s\r\n", 
                DbgDevicePowerString(TargetState));

    PAGED_CODE();

    deviceExtension = SerialGetDeviceExtension (Device);

    // If there is a debugger conflict, avoid accessing the hardware since
    // the UART driver is only present for preventing any application/driver
    // from muxing-out the debugger.

    if (deviceExtension->DebugPortInUse) {
        ASSERT(deviceExtension->FunctionConfigConnectionId.QuadPart != 0LL);
        goto End;
    }

    if (deviceExtension->DeviceIsOpened == TRUE) {
        LARGE_INTEGER charTime;

        SetDeviceIsOpened(deviceExtension, FALSE, TRUE);

        charTime.QuadPart = -SerialGetCharTime(deviceExtension).QuadPart;

        // Shut down the chip

        SerialDisableUART(deviceExtension);

        // Drain the device

        SerialDrainUART(deviceExtension, &charTime);

        // Save the device state

        SerialSaveDeviceState(deviceExtension);

    } else {
        SetDeviceIsOpened(deviceExtension, FALSE, FALSE);
    }

End:

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "--SerialEvtDeviceD0Exit\r\n");

    return STATUS_SUCCESS;
}

/*++

Routine Description:

    This routine saves the device state of the UART

Arguments:

    PDevExt - Pointer to the device extension for the devobj to save the state
              for.

Return Value:

    VOID


--*/
_Use_decl_annotations_
VOID
SerialSaveDeviceState(PSERIAL_DEVICE_EXTENSION PDevExt)
{
    PSERIAL_DEVICE_STATE pDevState = &PDevExt->DeviceState;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "++SerialSaveDeviceState\r\n");

    // Read necessary registers direct

    pDevState->IER = READ_INTERRUPT_ENABLE(PDevExt, PDevExt->Controller);
    pDevState->MCR = READ_MODEM_CONTROL(PDevExt, PDevExt->Controller);
    pDevState->LCR = READ_LINE_CONTROL(PDevExt, PDevExt->Controller);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "--SerialSaveDeviceState\r\n");
}

_Use_decl_annotations_
VOID
SetDeviceIsOpened(PSERIAL_DEVICE_EXTENSION PDevExt,
            BOOLEAN DeviceIsOpened,
            BOOLEAN Reopen)
{

    PDevExt->DeviceIsOpened     = DeviceIsOpened;
    PDevExt->DeviceState.Reopen = Reopen;
}



