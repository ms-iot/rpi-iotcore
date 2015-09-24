/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name: 

    internal.h

Abstract:

    This module contains the common internal type and function
    definitions for the BCM2838 SPI controller driver.

Environment:

    kernel-mode only

Revision History:

--*/

#ifndef _INTERNAL_H_
#define _INTERNAL_H_

#pragma warning(push)
#pragma warning(disable:4512)
#pragma warning(disable:4480)

#define BCMS_POOL_TAG ((ULONG) 'SMCB')

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
#include "spitrace.h"


/////////////////////////////////////////////////
//
// Hardware definitions.
//
/////////////////////////////////////////////////

#include "bcmspi.h"

/////////////////////////////////////////////////
//
// Resource and descriptor definitions.
//
/////////////////////////////////////////////////

#include "reshub.h"

//
// SPI Serial peripheral bus descriptor
//

#include "pshpack1.h"

typedef struct _PNP_SPI_SERIAL_BUS_DESCRIPTOR {
    PNP_SERIAL_BUS_DESCRIPTOR SerialBusDescriptor;
    // the SPI descriptor, see ACPI 5.0 spec table 6-192 
    ULONG ConnectionSpeed;
    UCHAR DataBitLength;
    UCHAR Phase;
    UCHAR Polarity;
    USHORT DeviceSelection;
    // follwed by optional Vendor Data
    // followed by PNP_IO_DESCRIPTOR_RESOURCE_NAME
} PNP_SPI_SERIAL_BUS_DESCRIPTOR, *PPNP_SPI_SERIAL_BUS_DESCRIPTOR;

#include "poppack.h"

// see section 6.4.3.8.2 of the ACPI 5.0 specification
#define I2C_SERIAL_BUS_TYPE     0x01
#define I2C_SERIAL_BUS_SPECIFIC_FLAG_10BIT_ADDRESS 0x01
#define SPI_SERIAL_BUS_TYPE     0x02
#define UART_SERIAL_BUS_TYPE    0x03
#define SPI_DEVICEPOLARITY_BIT  0x02    // 0=active low, 1=active high
#define SPI_WIREMODE_BIT        0x01    // 0=4 wires, 1=3 wires
#define SPI_SLV_BIT             0x01    // 0=initiated by controller, 1=by device

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

typedef struct PBC_TARGET_SETTINGS
{
    // from PNP_SERIAL_BUS_DESCRIPTOR
    USHORT TypeSpecificFlags;
    UCHAR GeneralFlags;
    // from PNP_SPI_SERIAL_BUS_DESCRIPTOR
    ULONG ConnectionSpeed;
    UCHAR DataBitLength;
    UCHAR Phase;
    UCHAR Polarity;
    USHORT DeviceSelection;
}
PBC_TARGET_SETTINGS, *PPBC_TARGET_SETTINGS;

/////////////////////////////////////////////////
//
// Context definitions.
//
/////////////////////////////////////////////////

typedef struct PBC_DEVICE PBC_DEVICE, *PPBC_DEVICE;
typedef struct PBC_TARGET PBC_TARGET, *PPBC_TARGET;
typedef struct PBC_REQUEST PBC_REQUEST, *PPBC_REQUEST;

//
// Device context.
//

struct PBC_DEVICE 
{
    // Handle to the WDF device.
    WDFDEVICE                       FxDevice;

    // SPI control block for this instance
    PBCM_SPI_REGISTERS              pSPIRegisters;
    ULONG                           SPIRegistersCb;
    PHYSICAL_ADDRESS                pSPIRegistersPhysicalAddress;

    // shadow copy of CS hardware register and clock speed
    ULONG                           SPI_CS_COPY;                 
    ULONG                           CurrentConnectionSpeed;

    // Target that the controller is currently
    // configured for. In most cases this value is only
    // set when there is a request being handled, however,
    // it will persist between lock and unlock requests.
    // There cannot be more than one current target.
    PPBC_TARGET                     pCurrentTarget;
    BOOLEAN                         Locked;
    
    // Controller driver spinlock.
    WDFSPINLOCK                     Lock;

    // The power setting callback handle
    PVOID                           pMonitorPowerSettingHandle;

    PVOID                           pTransferThread;
    KEVENT                          TransferThreadWakeEvt;
    LONG                            TransferThreadShutdown;
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
    ULONG                          CurrentTransferIndex;

    // Total bytes transferred.
    size_t                         TotalInformation;

    size_t                         RequestLength;

    //
    // Variables that are reused for each transfer within
    // each request.
    //

    size_t                         CurrentTransferWriteLength;
    size_t                         CurrentTransferReadLength;
    PMDL                           pCurrentTransferWriteMdlChain;
    PMDL                           pCurrentTransferReadMdlChain;

    // Bytes read/written in the current transfer.
    size_t                         CurrentTransferInformation;

    // Position of the current transfer within
    // the sequence and its associated controller
    // settings.
    SPB_REQUEST_SEQUENCE_POSITION  CurrentTransferSequencePosition;
    SPB_TRANSFER_DIRECTION         CurrentTransferDirection;
    ULONG                          CurrentTransferDelayInUs;
};

//
// Declate contexts for device, target, and request.
//

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PBC_DEVICE, GetDeviceContext);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PBC_TARGET, GetTargetContext);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PBC_REQUEST, GetRequestContext);

#pragma warning(pop)

#endif // _INTERNAL_H_
