/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:

    This module contains code to control the PWM module.

--*/

#include "driver.h"
#include "pwm.tmh"

#pragma code_seg()
_Must_inspect_result_
NTSTATUS
ValidateClockConfig(
    _In_ PBCM_PWM_CLOCK_CONFIG ClockConfig
)
/*++

Routine Description:

    This function checks if the requested PWM clock configuration is valid.

Arguments:

    ClockConfig - a pointer to the clock configuration

Return Value:

    Status

--*/
{

    NTSTATUS status = STATUS_SUCCESS;

    //
    // Validate the request parameter.
    //

    if (ClockConfig->ClockSource != BCM_PWM_CLOCKSOURCE_PLLC &&
        ClockConfig->ClockSource != BCM_PWM_CLOCKSOURCE_PLLD)
    {
        status = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Invalid clock source in clock configuration. (0x%08x)", (ULONG)ClockConfig->ClockSource);
    }

    if (ClockConfig->Divisor < 2 || (ClockConfig->Divisor & ~(CM_PWMDIV_DIVI_MASK>>CM_PWMDIV_DIVI_SHIFT)) != 0)
    {
        status = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Invalid divisor in clock configuration. (0x%08x)", ClockConfig->Divisor);
    }
    return status;
}

#pragma code_seg()
_Use_decl_annotations_
VOID
SetClockConfig(
    PDEVICE_CONTEXT DeviceContext
)
/*++

Routine Description:

    This function configures the PWM clock, using the values of the device context.

Arguments:

    DeviceContext - a pointer to the device context

Return Value:

    None

--*/
{
    ULONG i;

    //
    // Turn PWM clock off and reset it.
    //

    WRITE_REGISTER_ULONG(&DeviceContext->cmPwmRegs->PWMCTL, CM_PWMCTL_PASSWD);
    KeStallExecutionProcessor(10);
    for (i = 0; i < 10 && (READ_REGISTER_ULONG(&DeviceContext->cmPwmRegs->PWMCTL) & CM_PWMCTL_BUSY); i++)
    {
        KeStallExecutionProcessor(5);
    }
    if (i == 10)
    {
        WRITE_REGISTER_ULONG(&DeviceContext->cmPwmRegs->PWMCTL, CM_PWMCTL_PASSWD | CM_PWMCTL_KILL);
        for (i = 0; i < 10 && (READ_REGISTER_ULONG(&DeviceContext->cmPwmRegs->PWMCTL) & CM_PWMCTL_BUSY); i++)
        {
            KeStallExecutionProcessor(5);
        }
        if (i == 10)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "Can not reset PWM clock. Ignoring.");
        }
    }

    //
    // Setup the PWM clock divisor.
    //

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "Set clock divisor to 0x%08x", DeviceContext->pwmClockConfig.Divisor);
    WRITE_REGISTER_ULONG(&DeviceContext->cmPwmRegs->PWMDIV, ((DeviceContext->pwmClockConfig.Divisor << CM_PWMDIV_DIVI_SHIFT) | CM_PWMDIV_PASSWD));

    //
    // Turn the PWM clock on.
    //

    ULONG cmPwmCtlSrc;
    switch (DeviceContext->pwmClockConfig.ClockSource)
    {
    case BCM_PWM_CLOCKSOURCE_PLLC:
        cmPwmCtlSrc = CM_PWMCTL_SRC_PLLC;
        break;
    case BCM_PWM_CLOCKSOURCE_PLLD:
    default:
        cmPwmCtlSrc = CM_PWMCTL_SRC_PLLD;
        break;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "Set PWM clock source register to 0x%08x", cmPwmCtlSrc);
    WRITE_REGISTER_ULONG(&DeviceContext->cmPwmRegs->PWMCTL, (cmPwmCtlSrc | CM_PWMCTL_PASSWD));
    KeStallExecutionProcessor(10);
    WRITE_REGISTER_ULONG(&DeviceContext->cmPwmRegs->PWMCTL, (cmPwmCtlSrc | CM_PWMCTL_ENAB | CM_PWMCTL_PASSWD));

    return;
}


#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
ValidateAndSetClockConfig(
    WDFDEVICE Device,
    WDFREQUEST Request
)
/*++

Routine Description:

    This function validates PWM clock configuration and configures the PWM clock.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;
    PBCM_PWM_CLOCK_CONFIG clockConfig;

    deviceContext = GetContext(Device);

    //
    // Validate the request parameter.
    //

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(*clockConfig),
        (PVOID *)&clockConfig,
        NULL
        );

    if (NT_SUCCESS(status))
    {
        status = ValidateClockConfig(clockConfig);

        if (NT_SUCCESS(status))
        {
            //
            // Only allow change if PWM is in register mode and if none of the PWM channels is running.
            //

            if (deviceContext->pwmMode != PWM_MODE_REGISTER)
            {
                status = STATUS_OPERATION_IN_PROGRESS;
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "PWM is not in register mode Could not set clock configuration.");
            }
            else if (PWM_CHANNEL1_IS_RUNNING(deviceContext) || PWM_CHANNEL2_IS_RUNNING(deviceContext))
            {
                status = STATUS_OPERATION_IN_PROGRESS;
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Device is running. Change of clock configuration not allowed.");
            }

            //
            // Take over clock configuration and set it.
            //
            
            if (NT_SUCCESS(status))
            {
                WdfSpinLockAcquire(deviceContext->pwmLock);

                deviceContext->pwmClockConfig.ClockSource = clockConfig->ClockSource;
                deviceContext->pwmClockConfig.Divisor = clockConfig->Divisor;
                SetClockConfig(deviceContext);

                WdfSpinLockRelease(deviceContext->pwmLock);
            }
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Error retrieving clock config input buffer. (0x%08x)", status);
    }

    return status;
}

#pragma code_seg()
_Must_inspect_result_
NTSTATUS
ValidateChannelConfig(
    _In_ PBCM_PWM_CHANNEL_CONFIG ChannelConfig
)
/*++

Routine Description:

    This function validates the PWM channel.

Arguments:

    ChannelConfig - a pointer to the channel configuration

Return Value:

    Status

--*/
{
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Validate the request parameter.
    //

    if (IS_INVALID_CHANNEL(ChannelConfig->Channel))
    {
        status = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Invalid channel in channel configuration. (0x%08x)", (ULONG)ChannelConfig->Channel);
    }

    if (ChannelConfig->DutyMode != BCM_PWM_DUTYMODE_MARKSPACE &&
        ChannelConfig->DutyMode != BCM_PWM_DUTYMODE_PWM)
    {
        status = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Invalid duty mode channel configuration. (0x%08x)", (ULONG)ChannelConfig->DutyMode);
    }

    if (ChannelConfig->Mode != BCM_PWM_MODE_PWM &&
        ChannelConfig->Mode != BCM_PWM_MODE_SERIALISER)
    {
        status = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Invalid mode in channel configuration. (0x%08x)", (ULONG)ChannelConfig->Mode);
    }

    if (ChannelConfig->Polarity != BCM_PWM_POLARITY_NORMAL &&
        ChannelConfig->Polarity != BCM_PWM_POLARITY_INVERTED)
    {
        status = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Invalid polarity in channel configuration. (0x%08x)", (ULONG)ChannelConfig->Polarity);
    }

    if (ChannelConfig->Repeat != BCM_PWM_REPEATMODE_OFF &&
        ChannelConfig->Repeat != BCM_PWM_REPEATMODE_ON)
    {
        status = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Invalid repeat mode in channel configuration. (0x%08x)", (ULONG)ChannelConfig->Repeat);
    }

    if (ChannelConfig->Silence != BCM_PWM_SILENCELEVEL_LOW &&
        ChannelConfig->Silence != BCM_PWM_SILENCELEVEL_HIGH)
    {
        status = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Invalid silence level in channel configuration. (0x%08x)", (ULONG)ChannelConfig->Silence);
    }

    return status;
}

#pragma code_seg()
_Use_decl_annotations_
VOID
SetChannelConfig(
    PDEVICE_CONTEXT DeviceContext
)
/*++

Routine Description:

    This function sets the PWM channel configuration.

Arguments:

    DeviceContext - a pointer to the device context

Return Value:

    None

--*/
{
    //
    // Configure ramge.
    //

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "Set range register channel 1: 0x%08x, channel 2: 0x%08x", 
        DeviceContext->pwmChannel1Config.Range, DeviceContext->pwmChannel2Config.Range);
    WRITE_REGISTER_ULONG(&DeviceContext->pwmRegs->RNG1, DeviceContext->pwmChannel1Config.Range);
    KeStallExecutionProcessor(30);
    WRITE_REGISTER_ULONG(&DeviceContext->pwmRegs->RNG2, DeviceContext->pwmChannel2Config.Range);

    //
    // Other channel configuration is set when the channel is started.
    //

    return;
}


#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
ValidateAndSetChannelConfig(
    WDFDEVICE Device,
    WDFREQUEST Request
)
/*++

Routine Description:

    This function configures the PWM channel.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;
    PBCM_PWM_CHANNEL_CONFIG channelConfig;

    deviceContext = GetContext(Device);

    //
    // Validate the request parameter.
    //

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(*channelConfig),
        (PVOID *)&channelConfig,
        NULL
        );

    if (NT_SUCCESS(status))
    {
        status = ValidateChannelConfig(channelConfig);

        if (NT_SUCCESS(status))
        {
            WdfSpinLockAcquire(deviceContext->pwmLock);

            //
            // Only allow change if PWM is in register mode and if the PWM channel is not running.
            //

            if (deviceContext->pwmMode != PWM_MODE_REGISTER)
            {
                status = STATUS_OPERATION_IN_PROGRESS;
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "PWM is not in register mode. Could not set channel configuration.");
            }
            else if (IS_CHANNEL_1_OR_ALL(channelConfig->Channel) && PWM_CHANNEL1_IS_RUNNING(deviceContext))
            {
                status = STATUS_OPERATION_IN_PROGRESS;
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "PWM channel 1 is already running. Need to stop channel 1 first.");
            }
            else if (IS_CHANNEL_2_OR_ALL(channelConfig->Channel) && PWM_CHANNEL2_IS_RUNNING(deviceContext))
            {
                status = STATUS_OPERATION_IN_PROGRESS;
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "PWM channel 2 is already running. Need to stop channel 2 first.");
            }

            //
            // Take over channel configuration and set it.
            //

            if (NT_SUCCESS(status))
            {
                if (IS_CHANNEL_1_OR_ALL(channelConfig->Channel))
                {
                    deviceContext->pwmChannel1Config.Range = channelConfig->Range;
                    deviceContext->pwmChannel1Config.DutyMode = channelConfig->DutyMode;
                    deviceContext->pwmChannel1Config.Mode = channelConfig->Mode;
                    deviceContext->pwmChannel1Config.Polarity = channelConfig->Polarity;
                    deviceContext->pwmChannel1Config.Repeat = channelConfig->Repeat;
                    deviceContext->pwmChannel1Config.Silence = channelConfig->Silence;
                    deviceContext->pwmDuty1 = (channelConfig->Range > PWM_DUTY_REGISTER_DEFAULT) ? channelConfig->Range : PWM_DUTY_REGISTER_DEFAULT;
                }
                if (IS_CHANNEL_2_OR_ALL(channelConfig->Channel))
                {
                    deviceContext->pwmChannel2Config.Range = channelConfig->Range;
                    deviceContext->pwmChannel2Config.DutyMode = channelConfig->DutyMode;
                    deviceContext->pwmChannel2Config.Mode = channelConfig->Mode;
                    deviceContext->pwmChannel2Config.Polarity = channelConfig->Polarity;
                    deviceContext->pwmChannel2Config.Repeat = channelConfig->Repeat;
                    deviceContext->pwmChannel2Config.Silence = channelConfig->Silence;
                    deviceContext->pwmDuty2 = (channelConfig->Range > PWM_DUTY_REGISTER_DEFAULT) ? channelConfig->Range : PWM_DUTY_REGISTER_DEFAULT;
                }

                SetChannelConfig(deviceContext);
            }

            WdfSpinLockRelease(deviceContext->pwmLock);
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Error retrieving channel config input buffer. (0x%08x)", status);
    }

    return status;
}


#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
GetChannelConfig(
    WDFDEVICE Device,
    WDFREQUEST Request
)
/*++

Routine Description:

    This function returns the PWM channel configuration.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;
    PBCM_PWM_CHANNEL_CONFIG channelConfig;
    BCM_PWM_CHANNEL *channel;

    deviceContext = GetContext(Device);

    //
    // Validate the request parameter.
    //

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(*channel),
        (PVOID *)&channel,
        NULL
        );

    if (NT_SUCCESS(status))
    {
        if (IS_INVALID_CHANNEL(*channel) || IS_CHANNEL_ALL(*channel))
        {
            status = STATUS_INVALID_PARAMETER;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Need to specify valid channel to retrieve configuration information.");
        }
        else
        {
            status = WdfRequestRetrieveOutputBuffer(
                Request,
                sizeof(*channelConfig),
                (PVOID *)&channelConfig,
                NULL
                );

            if (NT_SUCCESS(status))
            {
                //
                // return the channel configuration.
                //

                WdfSpinLockAcquire(deviceContext->pwmLock);

                if (IS_CHANNEL_1(*channel))
                {
                    channelConfig->Range = deviceContext->pwmChannel1Config.Range;
                    channelConfig->DutyMode = deviceContext->pwmChannel1Config.DutyMode;
                    channelConfig->Mode = deviceContext->pwmChannel1Config.Mode;
                    channelConfig->Polarity = deviceContext->pwmChannel1Config.Polarity;
                    channelConfig->Repeat = deviceContext->pwmChannel1Config.Repeat;
                    channelConfig->Silence = deviceContext->pwmChannel1Config.Silence;
                }
                if (IS_CHANNEL_2(*channel))
                {
                    channelConfig->Range = deviceContext->pwmChannel2Config.Range;
                    channelConfig->DutyMode = deviceContext->pwmChannel2Config.DutyMode;
                    channelConfig->Mode = deviceContext->pwmChannel2Config.Mode;
                    channelConfig->Polarity = deviceContext->pwmChannel2Config.Polarity;
                    channelConfig->Repeat = deviceContext->pwmChannel2Config.Repeat;
                    channelConfig->Silence = deviceContext->pwmChannel2Config.Silence;
                }

                WdfSpinLockRelease(deviceContext->pwmLock);

                WdfRequestSetInformation(Request, sizeof(BCM_PWM_CHANNEL_CONFIG));
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Error retrieving get channel config output buffer. (0x%08x)", status);
            }
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Error retrieving get channel config input buffer. (0x%08x)", status);
    }

    return status;
}


#pragma code_seg()
_Must_inspect_result_
NTSTATUS
ValidateDutyRegister(
    _In_ PDEVICE_CONTEXT DeviceContext,
    _In_ PBCM_PWM_SET_DUTY_REGISTER SetDutyRegister
)
/*++

Routine Description:

    This function validates the duty register value.

Arguments:

    DeviceContext - a pointer to the device context
    SetDutyRegister - a pointer to the set duty register data

Return Value:

    Status

--*/
{
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Validate the request parameter.
    //

    if (IS_INVALID_CHANNEL(SetDutyRegister->Channel))
    {
        status = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Invalid channel in set duty register data. (0x%08x)", (ULONG)SetDutyRegister->Channel);
    }

    if (IS_CHANNEL_1_OR_ALL(SetDutyRegister->Channel))
    {
        if (SetDutyRegister->Duty > DeviceContext->pwmChannel1Config.Range)
        {
            status = STATUS_INVALID_PARAMETER;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Duty value for channel 1 larger than range of channel 1. (0x%08x, 0x%08x)", SetDutyRegister->Duty, DeviceContext->pwmChannel1Config.Range);
        }
    }

    if (IS_CHANNEL_2_OR_ALL(SetDutyRegister->Channel))
    {
        if (SetDutyRegister->Duty > DeviceContext->pwmChannel2Config.Range)
        {
            status = STATUS_INVALID_PARAMETER;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Duty value for channel 2 larger than range of channel 2. (0x%08x, 0x%08x)", SetDutyRegister->Duty, DeviceContext->pwmChannel2Config.Range);
        }
    }

    return status;
}


#pragma code_seg()
VOID
SetDutyRegister(
    _In_ PDEVICE_CONTEXT DeviceContext
)
/*++

Routine Description:

    This function sets the PWM duty register.

Arguments:

    DeviceContext - a pointer to the device context

Return Value:

    None

--*/
{
    //
    // Set PWM duty register.
    //

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "Set duty register - channel 1: 0x%08x, channel 2: 0x%08x", 
        DeviceContext->pwmDuty1, DeviceContext->pwmDuty2);
    WRITE_REGISTER_ULONG(&DeviceContext->pwmRegs->DAT1, DeviceContext->pwmDuty1);
    KeStallExecutionProcessor(30);
    WRITE_REGISTER_ULONG(&DeviceContext->pwmRegs->DAT2, DeviceContext->pwmDuty2);
}


#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
ValidateAndSetDutyRegister(
    WDFDEVICE Device,
    WDFREQUEST Request
)
/*++

Routine Description:

    This function sets the PWM duty data register.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;
    PBCM_PWM_SET_DUTY_REGISTER setDutyRegister;

    deviceContext = GetContext(Device);

    //
    // Validate the request parameter.
    //

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(*setDutyRegister),
        (PVOID *)&setDutyRegister,
        NULL
        );

    if (NT_SUCCESS(status))
    {
        WdfSpinLockAcquire(deviceContext->pwmLock);

        status = ValidateDutyRegister(deviceContext, setDutyRegister);

        if (NT_SUCCESS(status))
        {
            //
            // Only allow change if PWM is in register mode.
            //

            if (deviceContext->pwmMode != PWM_MODE_REGISTER)
            {
                status = STATUS_OPERATION_IN_PROGRESS;
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "PWM is not in register mode. Could not set duty cycle.");
            }
            else
            {
                //
                // Take over duty data and set it.
                //
                if (IS_CHANNEL_1_OR_ALL(setDutyRegister->Channel))
                {
                    deviceContext->pwmDuty1 = setDutyRegister->Duty;
                }
                if (IS_CHANNEL_2_OR_ALL(setDutyRegister->Channel))
                {
                    deviceContext->pwmDuty2 = setDutyRegister->Duty;
                }
                SetDutyRegister(deviceContext);
            }
        }

        WdfSpinLockRelease(deviceContext->pwmLock);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Error retrieving set duty register input buffer. (0x%08x)", status);
    }

    return status;
}


#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
GetDutyRegister(
    WDFDEVICE Device,
    WDFREQUEST Request
)
/*++

Routine Description:

    This function returns the PWM duty data register setting.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG *dutyRegister;
    BCM_PWM_CHANNEL *channel;

    deviceContext = GetContext(Device);

    //
    // Validate the request parameter.
    //

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(*channel),
        (PVOID *)&channel,
        NULL
        );

    if (NT_SUCCESS(status))
    {
        if (IS_INVALID_CHANNEL(*channel) || IS_CHANNEL_ALL(*channel))
        {
            status = STATUS_INVALID_PARAMETER;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Need to specify valid channel to retrieve configuration information.");
        }
        else
        {
            status = WdfRequestRetrieveOutputBuffer(
                Request,
                sizeof(*dutyRegister),
                (PVOID *)&dutyRegister,
                NULL
                );

            if (NT_SUCCESS(status))
            {
                //
                // return the duty register.
                //

                WdfSpinLockAcquire(deviceContext->pwmLock);

                if (IS_CHANNEL_1(*channel))
                {
                    *dutyRegister = deviceContext->pwmDuty1;
                }

                if (IS_CHANNEL_2(*channel))
                {
                    *dutyRegister = deviceContext->pwmDuty2;
                }

                WdfSpinLockRelease(deviceContext->pwmLock);

                WdfRequestSetInformation(Request, sizeof(ULONG));
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Error retrieving get duty register output buffer. (0x%08x)", status);
            }
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Error retrieving get duty register input buffer. (0x%08x)", status);
    }

    return status;
}

#pragma code_seg()
_Use_decl_annotations_
VOID
StartChannel(
    PDEVICE_CONTEXT DeviceContext,
    BCM_PWM_CHANNEL Channel
)
/*++

Routine Description:

    This function starts PWM channels.

Arguments:

    DeviceContext - a pointer to the device context
    Channel - the channel to start

Return Value:

    None

--*/
{
    ULONG pwm1Ctl = 0;
    ULONG pwm2Ctl = 0;
    ULONG pwmCtl = 0;

    //
    // Get current setting.
    //

    pwmCtl = READ_REGISTER_ULONG(&DeviceContext->pwmRegs->CTL);

    //
    // Prepare new setting.
    //

    pwmCtl &= ~PWM_CTL_CLRF1;

    if (IS_CHANNEL_1_OR_ALL(Channel))
    {
        pwmCtl &= ~(PWM_CTL_PWEN1 | PWM_CTL_MODE1 | PWM_CTL_RPTL1 | PWM_CTL_SBIT1 | PWM_CTL_POLA1 | PWM_CTL_USEF1 | PWM_CTL_MSEN1);
        if (DeviceContext->pwmChannel1Config.Mode == BCM_PWM_MODE_SERIALISER)
        {
            pwm1Ctl |= PWM_CTL_MODE1;
        }
        if (DeviceContext->pwmChannel1Config.Repeat == BCM_PWM_REPEATMODE_ON)
        {
            //
            // Repeat is not supported for audio mode (both channels use FIFO input).
            //

            if (DeviceContext->pwmMode == PWM_MODE_AUDIO)
            {
                pwm1Ctl |= PWM_CTL_RPTL1;
            }
        }
        if (DeviceContext->pwmChannel1Config.Silence == BCM_PWM_SILENCELEVEL_HIGH)
        {
            pwm1Ctl |= PWM_CTL_SBIT1;
        }
        if (DeviceContext->pwmChannel1Config.Polarity == BCM_PWM_POLARITY_INVERTED)
        {
            pwm1Ctl |= PWM_CTL_POLA1;
        }

        //
        // Enable PWM channel 1. For audio mode use FIFO and DMA.
        //

        if (DeviceContext->pwmMode == PWM_MODE_AUDIO)
        {
            pwm1Ctl |= PWM_CTL_USEF1 | PWM_CTL_CLRF1 | PWM_CTL_PWEN1;
            WRITE_REGISTER_ULONG(&DeviceContext->pwmRegs->DMAC, (ULONG)(PWM_DMAC_ENAB | PWM_DMAC_DREQ_12 | PWM_DMAC_PANIC_8));
        }
        else
        {
            pwm1Ctl |= PWM_CTL_PWEN1;
        }

        if (DeviceContext->pwmChannel1Config.DutyMode == BCM_PWM_DUTYMODE_MARKSPACE)
        {
            pwm1Ctl |= PWM_CTL_MSEN1;
        }
        pwmCtl |= pwm1Ctl;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "PWM channel 1 start with CTL: 0x%08x, RNG: 0x%08x (%d), DAT: 0x%08x (%d), Source: %s)",
            pwmCtl, READ_REGISTER_ULONG(&DeviceContext->pwmRegs->RNG1), READ_REGISTER_ULONG(&DeviceContext->pwmRegs->RNG1),
            READ_REGISTER_ULONG(&DeviceContext->pwmRegs->DAT1), READ_REGISTER_ULONG(&DeviceContext->pwmRegs->DAT1),
            DeviceContext->pwmMode == PWM_MODE_AUDIO ? "audio" : "register"
        );
    }

    if (IS_CHANNEL_2_OR_ALL(Channel))
    {
        pwmCtl &= ~(PWM_CTL_PWEN2 | PWM_CTL_MODE2 | PWM_CTL_RPTL2 | PWM_CTL_SBIT2 | PWM_CTL_POLA2 | PWM_CTL_USEF2 | PWM_CTL_MSEN2);
        if (DeviceContext->pwmChannel2Config.Mode == BCM_PWM_MODE_SERIALISER)
        {
            pwm2Ctl |= PWM_CTL_MODE2;
        }
        if (DeviceContext->pwmChannel2Config.Repeat == BCM_PWM_REPEATMODE_ON)
        {
            //
            // Repeat is not supported for audio mode (both channels use FIFO input).
            //

            if (DeviceContext->pwmMode == PWM_MODE_AUDIO)
            {
                pwm2Ctl |= PWM_CTL_RPTL2;
            }
        }
        if (DeviceContext->pwmChannel2Config.Silence == BCM_PWM_SILENCELEVEL_HIGH)
        {
            pwm2Ctl |= PWM_CTL_SBIT2;
        }
        if (DeviceContext->pwmChannel2Config.Polarity == BCM_PWM_POLARITY_INVERTED)
        {
            pwm2Ctl |= PWM_CTL_POLA2;
        }

        //
        // Enable PWM channel 2. For audio mode use FIFO and DMA.
        //

        if (DeviceContext->pwmMode == PWM_MODE_AUDIO)
        {
            pwm2Ctl |= PWM_CTL_USEF2 | PWM_CTL_CLRF1 | PWM_CTL_PWEN2;
            WRITE_REGISTER_ULONG(&DeviceContext->pwmRegs->DMAC, (ULONG)(PWM_DMAC_ENAB | PWM_DMAC_DREQ_12 | PWM_DMAC_PANIC_8));
        }
        else
        {
            pwm2Ctl |= PWM_CTL_PWEN2;
        }

        if (DeviceContext->pwmChannel2Config.DutyMode == BCM_PWM_DUTYMODE_MARKSPACE)
        {
            pwm2Ctl |= PWM_CTL_MSEN2;
        }
        pwmCtl |= pwm2Ctl;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "PWM channel 2 start with CTL: 0x%08x, RNG: 0x%08x (%d), DAT: 0x%08x (%d), Source: %s)",
            pwmCtl, READ_REGISTER_ULONG(&DeviceContext->pwmRegs->RNG2), READ_REGISTER_ULONG(&DeviceContext->pwmRegs->RNG2),
            READ_REGISTER_ULONG(&DeviceContext->pwmRegs->DAT2), READ_REGISTER_ULONG(&DeviceContext->pwmRegs->DAT2),
            DeviceContext->pwmMode == PWM_MODE_AUDIO ? "audio" : "register"
        );
    }

    //
    // Apply new setting to start PWM.
    //

    WRITE_REGISTER_ULONG(&DeviceContext->pwmRegs->CTL, pwmCtl);
}


#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
ValidateAndStartChannel(
    WDFDEVICE Device,
    WDFREQUEST Request
)
/*++

Routine Description:

    This function validates the start operation. Starting a channel is only allowed for channels
    configured using register data and if the channel is not running.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;
    BCM_PWM_CHANNEL *channel;

    deviceContext = GetContext(Device);

    //
    // Validate the request parameter.
    //

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(*channel),
        (PVOID *)&channel,
        NULL
        );

    if (NT_SUCCESS(status))
    {
        WdfSpinLockAcquire(deviceContext->pwmLock);

        //
        // Only allow if PWM is in register mode.
        //

        if (deviceContext->pwmMode != PWM_MODE_REGISTER)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "PWM is not in register mode. Not allowed to start PWM.");
            status = STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        //
        // Only allow to start, if the PWM channel is not running.
        //

        if (NT_SUCCESS(status) && IS_CHANNEL_1_OR_ALL(*channel) && PWM_CHANNEL1_IS_RUNNING(deviceContext))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "PWM channel 1 is already running.");
            status = STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        if (NT_SUCCESS(status) && IS_CHANNEL_2_OR_ALL(*channel) && PWM_CHANNEL2_IS_RUNNING(deviceContext))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "PWM channel 2 is already running.");
            status = STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        //
        // Start the channel, but not the DMA.
        //

        if (NT_SUCCESS(status))
        {
            StartChannel(deviceContext, *channel);
        }
    
        WdfSpinLockRelease(deviceContext->pwmLock);
    }
    return status;
}


#pragma code_seg()
_Use_decl_annotations_
VOID
StopChannel(
    PDEVICE_CONTEXT DeviceContext,
    BCM_PWM_CHANNEL Channel
)
/*++

Routine Description:

    This function stops PWM channels.

Arguments:

    DeviceContext - a pointer to the device context
    Channel - the channel to stop

Return Value:

    None

--*/
{
    ULONG pwmCtl;

    //
    // Get current setting.
    //

    pwmCtl = READ_REGISTER_ULONG(&DeviceContext->pwmRegs->CTL);

    //
    // Prepare new setting.
    //

    if (IS_CHANNEL_1_OR_ALL(Channel))
    {
        pwmCtl &= ~PWM_CTL_PWEN1;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "Stop PWM channel 1. (0x%08x)", pwmCtl);
    }

    if (IS_CHANNEL_2_OR_ALL(Channel))
    {
        pwmCtl &= ~PWM_CTL_PWEN2;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "Stop PWM channel 2. (0x%08x)", pwmCtl);
    }

    //
    // Apply new setting to stop the PWM channel.
    //

    WRITE_REGISTER_ULONG(&DeviceContext->pwmRegs->CTL, pwmCtl);
}


#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
ValidateAndStopChannel(
    WDFDEVICE Device,
    WDFREQUEST Request
)
/*++

Routine Description:

    This function stops PWM and DMA operation.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;
    BCM_PWM_CHANNEL *channel;

    deviceContext = GetContext(Device);

    //
    // Validate the request parameter.
    //

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(*channel),
        (PVOID *)&channel,
        NULL
        );

    if (NT_SUCCESS(status))
    {
        //
        // Only allow to stop if PWM is in register mode.
        //

        if (deviceContext->pwmMode != PWM_MODE_REGISTER)
        {
            status = STATUS_OPERATION_IN_PROGRESS;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "PWM is not in register mode. Could not stop PWM.");
        }
        else
        {
            WdfSpinLockAcquire(deviceContext->pwmLock);

            //
            // Stop the channel.
            //

            StopChannel(deviceContext, *channel);
            StopDma(deviceContext);

            WdfSpinLockRelease(deviceContext->pwmLock);
        }
    }
    return status;
}

#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
GetClockConfig(
    WDFDEVICE Device,
    WDFREQUEST Request
)
/*++

Routine Description:

    This function returns the PWM clock configuration.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;
    PBCM_PWM_CLOCK_CONFIG clockConfig;

    deviceContext = GetContext(Device);

    //
    // Validate the request parameter.
    //

    status = WdfRequestRetrieveOutputBuffer(
        Request,
        sizeof(*clockConfig),
        (PVOID *)&clockConfig,
        NULL
        );

    if (NT_SUCCESS(status))
    {
        //
        // return the clock configuration.
        //

        WdfSpinLockAcquire(deviceContext->pwmLock);

        clockConfig->ClockSource = deviceContext->pwmClockConfig.ClockSource;
        clockConfig->Divisor = deviceContext->pwmClockConfig.Divisor;

        WdfSpinLockRelease(deviceContext->pwmLock);

        WdfRequestSetInformation(Request, sizeof(BCM_PWM_CLOCK_CONFIG));
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Error retrieving clock config output buffer. (0x%08x)", status);
    }

    return status;
}

#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
AquireAudio(
    WDFDEVICE Device
)
/*++

Routine Description:

    This function puts the PWM driver in audio mode, if no PWM operation is active.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;

    deviceContext = GetContext(Device);

    WdfSpinLockAcquire(deviceContext->pwmLock);

    //
    // Only allow audio operation if PWM is not running in register mode.
    //

    if (deviceContext->pwmMode == PWM_MODE_REGISTER && (PWM_CHANNEL1_IS_RUNNING(deviceContext) || PWM_CHANNEL2_IS_RUNNING(deviceContext)))
    {
        status = STATUS_OPERATION_IN_PROGRESS;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "Device is running. Could not aquire PWM for audio operation.");
    }

    if (NT_SUCCESS(status))
    {
        //
        // Move PWM into audio mode.
        //

        deviceContext->pwmMode = PWM_MODE_AUDIO;

        //
        // Save PWM clock and channel configuration.
        //

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "Save PWM configuration to restore.");
        deviceContext->pwmSavedClockConfig = deviceContext->pwmClockConfig;
        deviceContext->pwmSavedChannel1Config = deviceContext->pwmChannel1Config;
        deviceContext->pwmSavedChannel2Config = deviceContext->pwmChannel2Config;
    }

    WdfSpinLockRelease(deviceContext->pwmLock);

    return status;
}


#pragma code_seg()
_Use_decl_annotations_
NTSTATUS
ReleaseAudio(
    WDFDEVICE Device
)
/*++

Routine Description:

    This function puts the PWM driver out of audio mode.

Arguments:

    Device - a pointer to the WDFDEVICE object
    Request - a pointer to the WDFREQUEST object

Return Value:

    Status

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;

    deviceContext = GetContext(Device);

    WdfSpinLockAcquire(deviceContext->pwmLock);

    //
    // Only release if PWM is in audio mode.
    //

    if (deviceContext->pwmMode != PWM_MODE_AUDIO)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "PWM is not in audio mode.");
    }
    else
    {
        //
        // Move PWM into register mode.
        //

        deviceContext->pwmMode = PWM_MODE_REGISTER;

        //
        // Restore PWM clock and channel configuration.
        //

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "Restore PWM configuration.");
        deviceContext->pwmClockConfig = deviceContext->pwmSavedClockConfig;
        deviceContext->pwmChannel1Config = deviceContext->pwmSavedChannel1Config;
        deviceContext->pwmChannel2Config = deviceContext->pwmSavedChannel2Config;
        SetClockConfig(deviceContext);
        SetChannelConfig(deviceContext);
    }

    WdfSpinLockRelease(deviceContext->pwmLock);

    return status;
}

