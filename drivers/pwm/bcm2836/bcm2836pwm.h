/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:

    This file contains the public device path names and
    IOCTL definitions for BCM2836 PWM driver.

--*/

#ifndef _BCM2836PWM_H
#define _BCM2836PWM_H

#if (NTDDI_VERSION >= NTDDI_WINTHRESHOLD)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//
// Device path names
//

#define BCM_PWM_NAME L"BCM2836PWM"

#define BCM_PWM_SYMBOLIC_NAME L"\\DosDevices\\" BCM_PWM_NAME
#define BCM_PWM_USERMODE_PATH L"\\\\.\\" BCM_PWM_NAME
#define BCM_PWM_USERMODE_PATH_SIZE sizeof(BCM_PWM_USERMODE_PATH)

//
// IOCTL codes
//

#define FILE_DEVICE_PWM_PERIPHERAL 0x400

//
// Set PWM clock configuration
//
// Input buffer:
// lpInBuffer - pointer to a variable of type BCM_PWM_CLOCK_CONFIG
// nInBufferSize - sizeof(BCM_PWM_CLOCK_CONFIG)
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_SET_CLOCKCONFIG               CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x700, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Get PWM clock configuration
//
// Input buffer:
// None
//
// Output buffer:
// lpOutBuffer - pointer to a variable of type BCM_PWM_CLOCK_CONFIG
// nOutBufferSize - sizeof(BCM_PWM_CLOCK_CONFIG)
//
#define IOCTL_BCM_PWM_GET_CLOCKCONFIG               CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x701, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Set PWM channel configuration
// The field channel in the input variable specifies for which channel the configuration should be applied.
//
// Input buffer:
// lpInBuffer - pointer to a variable of type BCM_PWM_CHANNEL_CONFIG
// nInBufferSize - sizeof(BCM_PWM_CHANNEL_CONFIG)
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_SET_CHANNELCONFIG             CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x702, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Get PWM channel configuration
// The input value of type BCM_PWM_CHANNEL specifies for which channel the configuration should be returned.
// If this field is set to a value different than BCM_PWM_CHANNEL_CHANNEL1 or BCM_PWM_CHANNEL_CHANNEL2,
// then STATUS_INVALID_PARAMETER is returned.
//
// Input buffer:
// lpInBuffer - pointer to a variable of type BCM_PWM_CHANNEL
// nInBufferSize - sizeof(BCM_PWM_CHANNEL)
//
// Output buffer:
// lpOutBuffer - pointer to a struct of type BCM_PWM_CHANNEL_CONFIG
// nOutBufferSize - sizeof(BCM_PWM_CHANNEL_CONFIG)
//
#define IOCTL_BCM_PWM_GET_CHANNELCONFIG             CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x703, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Set PWM duty register value
// The field channel in the input variable specifies for which channel the duty value should be set.
//
// Input buffer:
// lpInBuffer - pointer to a variable of type BCM_PWM_SET_DUTY_REGISTER
// nInBufferSize - sizeof(BCM_PWM_SET_DUTY_REGISTER)
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_SET_DUTY_REGISTER             CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x704, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Get duty register value
// The input value of type BCM_PWM_CHANNEL specifies for which channel the configuration should be returned.
// If this field is set to a value different than BCM_PWM_CHANNEL_CHANNEL1 or BCM_PWM_CHANNEL_CHANNEL2,
// then STATUS_INVALID_PARAMETER is returned.
//
// Input buffer:
// lpInBuffer - pointer to a variable of type BCM_PWM_CHANNEL
// nInBufferSize - sizeof(BCM_PWM_CHANNEL)
//
// Output buffer:
// lpOutBuffer - pointer to a variable of type BCM_PWM_DUTY_REGISTER
// nOutBufferSize - sizeof(BCM_PWM_DUTY_REGISTER)
//
#define IOCTL_BCM_PWM_GET_DUTY_REGISTER             CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x705, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Start PWM signal generator
// The input value of type BCM_PWM_CHANNEL specifies for which channel the generator should be started.
// If the channels are not configured using the duty register or if the PWM generator of the specified channel(s)
// is already running, STATUS_DEVICE_CONFIGURATION_ERROR is returned.
//
// Input buffer:
// lpInBuffer - pointer to a variable of type BCM_PWM_CHANNEL
// nInBufferSize - sizeof(BCM_PWM_CHANNEL)
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_START                         CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x706, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Stop PWM signal generator
// The input value of type BCM_PWM_CHANNEL specifies for which channel the generator should be stopped.
// If the channels are not configured using the duty register or write via IOCTL calls or if the PWM generator
// of the specified channel(s) is already stopped, STATUS_DEVICE_CONFIGURATION_ERROR is returned.
//
// Input buffer:
// lpInBuffer - pointer to a variable of type BCM_PWM_CHANNEL
// nInBufferSize - sizeof(BCM_PWM_CHANNEL)
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_STOP                          CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x707, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Aquire PWM for audio operation. This will prevent any register PWM calls till PWM is released with an 
// IOCTL_BCM_PWM_RELEASE_AUDIO call. PWM clock and channel settings are saved and restored in the IOCTL_BCM_PWM_RELEASE_AUDIO
// call.
// 
// Input buffer:
// None
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_AQUIRE_AUDIO                  CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x708, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Put PWM back into register operation mode and restores the clock and channel settings to the values before the
// IOCTL_BCM_PWM_AQUIRE_AUDIO call.
// 
// Input buffer:
// None
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_RELEASE_AUDIO                 CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x709, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Initializes PWM for audio playback. This includes configuration of the PWM channels and setup of the DMA control blocks.
// 
// Input buffer:
// lpInBuffer - pointer to a variable of type BCM_PWM_AUDIO_CONFIG
// nInBufferSize - sizeof(BCM_PWM_AUDIO_CONFIG)
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_INITIALIZE_AUDIO              CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x70A, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Register an event for notification by the driver. During the allocation the driver receives the number of
// notifications sent per buffer.
// Note: this call is only working if called from kernel mode.
// 
// Input buffer:
// lpInBuffer - pointer to a event handle
// nInBufferSize - sizeof an event handle
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_REGISTER_AUDIO_NOTIFICATION   CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x70B, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Unregister an event for notification by the driver.
// 
// Input buffer:
// lpInBuffer - pointer to a event handle
// nInBufferSize - sizeof an event handle
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_UNREGISTER_AUDIO_NOTIFICATION CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x70C, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Start audio DMA for both channels of the PWM controller.
// 
// Input buffer:
// None
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_START_AUDIO                   CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x70D, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Stop audio DMA for both channels of the PWM controller.
// 
// Input buffer:
// None
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_STOP_AUDIO                    CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x70E, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Pause audio DMA for both channels of the PWM controller.
// 
// Input buffer:
// None
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_PAUSE_AUDIO                   CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x70F, METHOD_BUFFERED, FILE_WRITE_DATA)

//
// Resume audio DMA for both channels of the PWM controller.
// 
// Input buffer:
// None
//
// Output buffer:
// None
//
#define IOCTL_BCM_PWM_RESUME_AUDIO                  CTL_CODE(FILE_DEVICE_PWM_PERIPHERAL, 0x710, METHOD_BUFFERED, FILE_WRITE_DATA)


typedef enum _BCM_PWM_CHANNEL {
    BCM_PWM_CHANNEL_CHANNEL1,
    BCM_PWM_CHANNEL_CHANNEL2,
    BCM_PWM_CHANNEL_ALLCHANNELS,
} BCM_PWM_CHANNEL;

typedef enum _BCM_PWM_DUTYMODE {
    BCM_PWM_DUTYMODE_MARKSPACE,
    BCM_PWM_DUTYMODE_PWM,
} BCM_PWM_DUTYMODE;

typedef enum _BCM_PWM_REPEATMODE {
    BCM_PWM_REPEATMODE_OFF,
    BCM_PWM_REPEATMODE_ON,
} BCM_PWM_REPEATMODE;

typedef enum _BCM_PWM_POLARITY {
    BCM_PWM_POLARITY_NORMAL,
    BCM_PWM_POLARITY_INVERTED,
} BCM_PWM_POLARITY;

typedef enum _BCM_PWM_SILENCELEVEL {
    BCM_PWM_SILENCELEVEL_LOW,
    BCM_PWM_SILENCELEVEL_HIGH,
} BCM_PWM_SILENCELEVEL;

typedef enum _BCM_PWM_MODE {
    BCM_PWM_MODE_PWM,
    BCM_PWM_MODE_SERIALISER,
} BCM_PWM_MODE;

typedef enum _BCM_PWM_CLOCKSOURCE {
    BCM_PWM_CLOCKSOURCE_PLLC,
    BCM_PWM_CLOCKSOURCE_PLLD,
} BCM_PWM_CLOCKSOURCE;

typedef struct _BCM_PWM_CLOCK_CONFIG {
    BCM_PWM_CLOCKSOURCE     ClockSource;
    ULONG                   Divisor;
} BCM_PWM_CLOCK_CONFIG, *PBCM_PWM_CLOCK_CONFIG;

typedef struct _BCM_PWM_CHANNEL_CONFIG {
    BCM_PWM_CHANNEL         Channel;
    ULONG                   Range;
    BCM_PWM_DUTYMODE        DutyMode;
    BCM_PWM_MODE            Mode;
    BCM_PWM_POLARITY        Polarity;
    BCM_PWM_REPEATMODE      Repeat;
    BCM_PWM_SILENCELEVEL    Silence;
} BCM_PWM_CHANNEL_CONFIG, *PBCM_PWM_CHANNEL_CONFIG;

typedef struct _BCM_PWM_SET_DUTY_REGISTER {
    BCM_PWM_CHANNEL         Channel;
    ULONG                   Duty;
} BCM_PWM_SET_DUTY_REGISTER, *PBCM_PWM_SET_DUTY_REGISTER;

typedef struct _BCM_PWM_PACKET_LINK_INFO {
    PVOID                   LinkPtr;
    ULONG                   LinkValue;
} BCM_PWM_PACKET_LINK_INFO, *PBCM_PWM_PACKET_LINK_INFO;

typedef struct _BCM_PWM_AUDIO_CONFIG {
    ULONG                   RequestedBufferSize;
    ULONG                   NotificationsPerBuffer;
    ULONG                   PwmRange;
    PVOID                   DmaBuffer;
    PBOOLEAN                DmaRestartRequired;
    PBCM_PWM_PACKET_LINK_INFO DmaPacketLinkInfo;
    ULONG                   DmaNumPackets;
    PULONG                  DmaPacketsInUse;
    PULONG                  DmaPacketsToPrime;
    PULONG                  DmaPacketsProcessed;
    PLARGE_INTEGER          DmaLastProcessedPacketTime;
} BCM_PWM_AUDIO_CONFIG, *PBCM_PWM_AUDIO_CONFIG;

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif

#endif