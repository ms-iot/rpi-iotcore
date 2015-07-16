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
    _In_     PPBC_DEVICE   pDevice,
    _Inout_  PPBC_REQUEST  pRequest);
    
VOID
ControllerCompleteTransfer(
    _Inout_  PPBC_DEVICE   pDevice,
    _Inout_  PPBC_REQUEST  pRequest,
    _In_     BOOLEAN       AbortSequence);

VOID
ControllerUnlockTransfer(
    _In_     PPBC_DEVICE   pDevice
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
    _In_     PPBC_DEVICE   pDevice,
    _In_     ULONG         InterruptMask);

VOID
ControllerAcknowledgeInterrupts(
    _In_     PPBC_DEVICE   pDevice,
    _In_     ULONG         InterruptStatus);

VOID
ControllerProcessInterrupts(
    _Inout_  PPBC_DEVICE   pDevice,
    _Inout_  PPBC_REQUEST  pRequest,
    _In_     ULONG         InterruptStatus);

#endif
