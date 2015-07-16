/*++

    Raspberry Pi 2 (BCM2836) I2C Controller driver for SPB Framework

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

    internal.h

Abstract:

    This module contains the common internal type and function
    definitions for the BCM2841 SPB I2C controller driver.

Environment:

    kernel-mode only

Revision History:

--*/

#ifndef _INTERNAL_H_
#define _INTERNAL_H_

#pragma warning(push)
#pragma warning(disable:4512)
#pragma warning(disable:4480)

#define BCMI_POOL_TAG ((ULONG) 'IMCB')

/////////////////////////////////////////////////
//
// Common includes.
//
/////////////////////////////////////////////////

#include <initguid.h>
#include <ntddk.h>
#include <wdm.h>
#include <wdf.h>
#include <ntstrsafe.h>

#include "SPBCx.h"
#include "i2ctrace.h"


/////////////////////////////////////////////////
//
// Hardware definitions.
//
/////////////////////////////////////////////////

#include "bcmi2c.h"

/////////////////////////////////////////////////
//
// Resource and descriptor definitions.
//
/////////////////////////////////////////////////

#include "reshub.h"

//
// I2C Serial peripheral bus descriptor
//

#include "pshpack1.h"

typedef struct _PNP_I2C_SERIAL_BUS_DESCRIPTOR {
    PNP_SERIAL_BUS_DESCRIPTOR SerialBusDescriptor;
    // the I2C descriptor, see ACPI 5.0 spec table 6-192 
    ULONG ConnectionSpeed;
    USHORT SlaveAddress;
    // follwed by optional Vendor Data
    // followed by PNP_IO_DESCRIPTOR_RESOURCE_NAME
} PNP_I2C_SERIAL_BUS_DESCRIPTOR, *PPNP_I2C_SERIAL_BUS_DESCRIPTOR;

#include "poppack.h"

// see section 6.4.3.8.2 of the ACPI 5.0 specification
#define I2C_SERIAL_BUS_TYPE                         0x01
#define I2C_SERIAL_BUS_SPECIFIC_FLAG_10BIT_ADDRESS  0x01
#define SPI_SERIAL_BUS_TYPE                         0x02
#define UART_SERIAL_BUS_TYPE                        0x03
#define I2C_SLV_BIT                                 0x01    // 0=initiated by controller, 1=by device
#define I2C_MIN_CONNECTION_SPEED                    BCM_I2C_CLOCK_RATE_LOWEST
#define I2C_MAX_CONNECTION_SPEED                    BCM_I2C_CLOCK_RATE_FAST
#define I2C_MAX_ADDRESS                             0x7f


/////////////////////////////////////////////////
//
// Settings.
//
/////////////////////////////////////////////////

//
// Power settings.
//

#define MONITOR_POWER_ON         1
#define MONITOR_POWER_OFF        0

#define IDLE_TIMEOUT_MONITOR_ON  2000
#define IDLE_TIMEOUT_MONITOR_OFF 50

//
// Target settings.
//

typedef enum ADDRESS_MODE
{
    AddressMode7Bit,
    AddressMode10Bit
}
ADDRESS_MODE, *PADDRESS_MODE;

typedef struct PBC_TARGET_SETTINGS
{
    // from PNP_SERIAL_BUS_DESCRIPTOR
    USHORT                        TypeSpecificFlags;
    UCHAR                         GeneralFlags;
    // from PNP_I2C_SERIAL_BUS_DESCRIPTOR
    ADDRESS_MODE                  AddressMode;
    USHORT                        Address;
    ULONG                         ConnectionSpeed;
}
PBC_TARGET_SETTINGS, *PPBC_TARGET_SETTINGS;


//
// Transfer settings. 
//

typedef enum BUS_CONDITION
{
    BusConditionFree,
    BusConditionBusy,
    BusConditionDontCare
}
BUS_CONDITION, *PBUS_CONDITION;

typedef struct PBC_TRANSFER_SETTINGS
{
    BUS_CONDITION                  BusCondition;
    BOOLEAN                        IsStart;
    BOOLEAN                        IsEnd;
}
PBC_TRANSFER_SETTINGS, *PPBC_TRANSFER_SETTINGS;

/////////////////////////////////////////////////
//
// Context definitions.
//
/////////////////////////////////////////////////

typedef struct PBC_DEVICE   PBC_DEVICE,   *PPBC_DEVICE;
typedef struct PBC_TARGET   PBC_TARGET,   *PPBC_TARGET;
typedef struct PBC_REQUEST  PBC_REQUEST,  *PPBC_REQUEST;

//
// Device context.
//

struct PBC_DEVICE 
{
    // Handle to the WDF device.
    WDFDEVICE                      FxDevice;

    // I2C control block for this instance
    PBCM_I2C_REGISTERS             pRegisters;
    ULONG                          RegistersCb;
    PHYSICAL_ADDRESS               pRegistersPhysicalAddress;

    // shadow copy of CS hardware register and clock speed
    ULONG                          I2C_CONTROL_COPY;                 
    ULONG                          CurrentConnectionSpeed;

    // Target that the controller is currently
    // configured for. In most cases this value is only
    // set when there is a request being handled, however,
    // it will persist between lock and unlock requests.
    // There cannot be more than one current target.
    PPBC_TARGET                    pCurrentTarget;
    
    // Variables to track enabled interrupts
    // and status between ISR and DPC.
    WDFINTERRUPT                   InterruptObject;
    ULONG                          InterruptMask;
    ULONG                          InterruptStatus;

    // Controller driver spinlock.
    WDFSPINLOCK                    Lock;

    // Delay timer used to stall between transfers.
    WDFTIMER                       DelayTimer;

    // The power setting callback handle
    PVOID                          pMonitorPowerSettingHandle;
};

//
// Target context.
//

struct PBC_TARGET 
{
    // Handle to the SPB target.
    SPBTARGET                      SpbTarget;

    // Target specific settings.
    PBC_TARGET_SETTINGS            Settings;
    
    // Current request associated with the 
    // target. This value should only be non-null
    // when this target is the controller's current
    // target.
    PPBC_REQUEST                   pCurrentRequest;
};

//
// Request context.
//

struct PBC_REQUEST 
{
    //
    // Variables that persist for the lifetime of
    // the request. Specifically these apply to an
    // entire sequence request (not just a single transfer).
    //

    // Handle to the SPB request.
    SPBREQUEST                     SpbRequest;

    // SPB request type.
    SPB_REQUEST_TYPE               Type;

    // Number of transfers in sequence and 
    // index of the current one.
    ULONG                          TransferCount; 
    ULONG                          TransferIndex;

    // Total bytes transferred.
    size_t                         TotalInformation;

    // Current status of the request.
    NTSTATUS                       Status;
    BOOLEAN                        bIoComplete;

    //
    // Variables that are reused for each transfer within
    // a [sequence] request.
    //

    // Pointer to the transfer buffer and length.
    size_t                         Length;
    PMDL                           pMdlChain;

    // Position of the current transfer within
    // the sequence and its associated controller
    // settings.
    SPB_REQUEST_SEQUENCE_POSITION  SequencePosition;
    PBC_TRANSFER_SETTINGS          Settings;
    BOOLEAN                        RepeatedStart;

    // Direction of the current transfer.
    SPB_TRANSFER_DIRECTION         Direction;

    // Time to delay before starting transfer.
    ULONG                          DelayInUs;

    // Interrupt flag indicating data is ready to
    // be transferred.
    ULONG                          DataReadyFlag; 

    // Bytes read/written in the current transfer.
    size_t                         Information;
};

//
// Declate contexts for device, target, and request.
//

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PBC_DEVICE,  GetDeviceContext);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PBC_TARGET,  GetTargetContext);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PBC_REQUEST, GetRequestContext);

//
// Register evaluation functions.
//

FORCEINLINE
bool
TestAnyBits(
    _In_ ULONG V1,
    _In_ ULONG V2
)
{
    return (V1 & V2) != 0;
}


#pragma warning(pop)

#endif // _INTERNAL_H_
