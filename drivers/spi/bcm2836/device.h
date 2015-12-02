/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name: 

    device.h

Abstract:

    This module contains the function definitions for the 
    WDF device.

Environment:

    kernel-mode only

Revision History:

--*/

#ifndef _DEVICE_H_
#define _DEVICE_H_

//
// WDF event callbacks.
//

EVT_WDF_DEVICE_PREPARE_HARDWARE         OnPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE         OnReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY                 OnD0Entry;
EVT_WDF_DEVICE_D0_EXIT                  OnD0Exit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT     OnSelfManagedIoInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP  OnSelfManagedIoCleanup;

EVT_WDF_REQUEST_CANCEL                  OnCancel;

//
// Power framework event callbacks.
//

__drv_functionClass(POWER_SETTING_CALLBACK)
_IRQL_requires_same_
NTSTATUS
OnMonitorPowerSettingCallback(
    _In_ LPCGUID SettingGuid,
    _In_reads_bytes_(ValueLength) PVOID Value,
    _In_  ULONG ValueLength,
    _Inout_opt_  PVOID Context
   );

//
// SPBCx event callbacks.
//

EVT_SPB_TARGET_CONNECT               OnTargetConnect;
EVT_SPB_CONTROLLER_LOCK              OnControllerLock;
EVT_SPB_CONTROLLER_UNLOCK            OnControllerUnlock;
EVT_SPB_CONTROLLER_READ              OnRead;
EVT_SPB_CONTROLLER_WRITE             OnWrite;
EVT_SPB_CONTROLLER_SEQUENCE          OnSequenceRequest;

EVT_WDF_IO_IN_CALLER_CONTEXT         OnOtherInCallerContext;
EVT_SPB_CONTROLLER_OTHER             OnOther;

KSTART_ROUTINE TransferPollModeThread;

//
// PBC function prototypes.
//

VOID
OnNonSequenceRequest(
    _In_ WDFDEVICE SpbController,
    _In_ SPBTARGET SpbTarget,
    _In_ SPBREQUEST SpbRequest,
    _In_ size_t Length
    );

NTSTATUS
OnRequest(
    _In_ PPBC_DEVICE pDevice,
    _In_ PPBC_TARGET pTarget,
    _In_ PPBC_REQUEST pRequest
    );

VOID
OnRequestPollMode(
    _In_ PPBC_DEVICE pDevice
    );

NTSTATUS
PbcTargetGetSettings(
    _In_ PPBC_DEVICE  pDevice,
    _In_ PVOID ConnectionParameters,
    _Out_ PPBC_TARGET_SETTINGS pSettings
    );

NTSTATUS
PbcRequestSetNthTransferInfo(
    _Inout_ PPBC_REQUEST pRequest,
    _In_ ULONG Index
    );

VOID
PbcRequestDoTransfer(
    _In_ PPBC_DEVICE pDevice,
    _In_ PPBC_REQUEST pRequest
    );

NTSTATUS
FORCEINLINE
MdlGetByte(
    _In_ PMDL mdl,
    _In_ size_t Index,
    _In_ size_t Length,
   _Out_ UCHAR* pByte
   )
/*++
 
  Routine Description:

    This is a helper routine used to retrieve the 
    specified byte of the current transfer descriptor buffer.

  Arguments:

    pRequest - a pointer to the PBC request context

    Index - index of desired byte in current transfer descriptor buffer

    pByte - pointer to the location for the specified byte

  Return Value:

    STATUS_INFO_LENGTH_MISMATCH if invalid index, 
    otherwise STATUS_SUCCESS

--*/
{
    size_t mdlByteCount;
    size_t currentOffset = Index;
    PUCHAR pBuffer;
    NTSTATUS status = STATUS_INFO_LENGTH_MISMATCH;

    //
    // Check for out-of-bounds index
    //

    if (Index < Length)
    {
        while (mdl != NULL)
        {
            mdlByteCount = MmGetMdlByteCount(mdl);

            if (currentOffset < mdlByteCount)
            {
                pBuffer = (PUCHAR) MmGetSystemAddressForMdlSafe(
                    mdl, 
                    NormalPagePriority);

                if (pBuffer != NULL)
                {
                    //
                    // Byte found, mark successful
                    //

                    *pByte = pBuffer[currentOffset];
                    status = STATUS_SUCCESS;
                }

                break;
            }

            currentOffset -= mdlByteCount;
            mdl = mdl->Next;
        }

        //
        // If after walking the MDL the byte hasn't been found,
        // status will still be STATUS_INFO_LENGTH_MISMATCH
        //
    }

    return status;
}

NTSTATUS
FORCEINLINE
MdlSetByte(
   _In_ PMDL mdl,
   _In_ size_t Index,
   _In_ size_t Length,
   _In_ UCHAR Byte
   )
/*++
 
  Routine Description:

    This is a helper routine used to set the 
    specified byte of the current transfer descriptor buffer.

  Arguments:

    pRequest - a pointer to the PBC request context

    Index - index of desired byte in current transfer descriptor buffer

    Byte - the byte

  Return Value:

    STATUS_INFO_LENGTH_MISMATCH if invalid index, 
    otherwise STATUS_SUCCESS

--*/
{
    size_t mdlByteCount;
    size_t currentOffset = Index;
    PUCHAR pBuffer;
    NTSTATUS status = STATUS_INFO_LENGTH_MISMATCH;

    //
    // Check for out-of-bounds index
    //

    if (Index < Length)
    {
        while (mdl != NULL)
        {
            mdlByteCount = MmGetMdlByteCount(mdl);

            if (currentOffset < mdlByteCount)
            {
                pBuffer = (PUCHAR) MmGetSystemAddressForMdlSafe(
                    mdl, 
                    NormalPagePriority);

                if (pBuffer != NULL)
                {
                    //
                    // Byte found, mark successful
                    //

                    pBuffer[currentOffset] = Byte;
                    status = STATUS_SUCCESS;
                }

                break;
            }

            currentOffset -= mdlByteCount;
            mdl = mdl->Next;
        }

        //
        // If after walking the MDL the byte hasn't been found,
        // status will still be STATUS_INFO_LENGTH_MISMATCH
        //
    }

    return status;
}

#endif
