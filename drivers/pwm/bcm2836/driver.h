/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Abstract:

Header file which includes all other header files.

--*/


#define INITGUID

// Windows related header files
#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>

// Implementation header files
#include "bcm2836pwm.h"
#include "gpio.h"
#include "clockmgr.h"
#include "pwm.h"
#include "dma.h"
#include "dmaInterrupt.h"
#include "device.h"
#include "trace.h"

#define BCM_PWM_POOLTAG 'PMCB'

extern "C" DRIVER_INITIALIZE DriverEntry;
extern "C" EVT_WDF_DRIVER_UNLOAD OnDriverUnload;
