/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name: 

    controller.h

Abstract:

    This module contains the controller-specific function
    definitions.

Environment:

    kernel-mode only

Revision History:

--*/

#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

//
// Controller specific function prototypes.
//

VOID ControllerInitialize(
    _Inout_  PPBC_DEVICE   pDevice);

VOID ControllerUninitialize(
    _Inout_  PPBC_DEVICE   pDevice);

VOID
ControllerConfigureForTransfer(
    _Inout_  PPBC_DEVICE   pDevice,
    _Inout_  PPBC_REQUEST  pRequest);

NTSTATUS
ControllerTransferData(
    _Inout_  PPBC_DEVICE   pDevice,
    _Inout_  PPBC_REQUEST  pRequest);
    
VOID
ControllerCompleteTransfer(
    _Inout_  PPBC_DEVICE   pDevice,
    _Inout_  PPBC_REQUEST  pRequest,
    _In_     BOOLEAN       AbortSequence);

VOID
ControllerUnlockTransfer(
    _Inout_  PPBC_DEVICE   pDevice
);

VOID
ControllerEnableInterrupts(
    _Inout_  PPBC_DEVICE   pDevice,
    _In_     ULONG         InterruptMask);

VOID
ControllerDisableInterrupts(
    _Inout_  PPBC_DEVICE   pDevice);

ULONG
ControllerGetInterruptStatus(
    _In_  PPBC_DEVICE   pDevice,
    _In_  ULONG         InterruptMask);

VOID
ControllerAcknowledgeInterrupts(
    _In_  PPBC_DEVICE   pDevice,
    _In_  ULONG         InterruptMask);

VOID
ControllerProcessInterrupts(
    _Inout_ PPBC_DEVICE   pDevice,
    _Inout_ PPBC_REQUEST  pRequest,
    _In_    ULONG         InterruptStatus);

#endif
