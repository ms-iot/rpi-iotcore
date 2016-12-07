// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    qsfile.c
//
// Abstract:
//
//   This module contains the code that is very specific to query/set file
//    operations in the serial driver.
//

#include "precomp.h"
#include "qsfile.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGESRP0,SerialQueryInformationFile)
#pragma alloc_text(PAGESRP0,SerialSetInformationFile)
#endif

/*++

Routine Description:

    This routine is used to query the end of file information on
    the opened serial port.  Any other file information request
    is retured with an invalid parameter.

    This routine always returns an end of file of 0.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    The function value is the final status of the call

--*/
_Use_decl_annotations_
NTSTATUS
SerialQueryInformationFile(
    WDFDEVICE Device,
    PIRP Irp
    )
{
    NTSTATUS status;
    PIO_STACK_LOCATION IrpSp;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "++SerialQueryInformationFile(%p, %p)\r\n",
                Device,
                Irp);

    PAGED_CODE();

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Irp->IoStatus.Information = 0L;
    status = STATUS_SUCCESS;

    if (IrpSp->Parameters.QueryFile.FileInformationClass ==
        FileStandardInformation) {

        if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(FILE_STANDARD_INFORMATION))
        {
                status = STATUS_BUFFER_TOO_SMALL;

        } else {

            PFILE_STANDARD_INFORMATION buf = Irp->AssociatedIrp.SystemBuffer;

            buf->AllocationSize.QuadPart = 0;
            buf->EndOfFile = buf->AllocationSize;
            buf->NumberOfLinks = 0;
            buf->DeletePending = FALSE;
            buf->Directory = FALSE;
            Irp->IoStatus.Information = sizeof(FILE_STANDARD_INFORMATION);
        }

    } else if (IrpSp->Parameters.QueryFile.FileInformationClass ==
               FilePositionInformation) {

        if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(FILE_POSITION_INFORMATION)) {

                status = STATUS_BUFFER_TOO_SMALL;

        } else {

            ((PFILE_POSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer)->
                CurrentByteOffset.QuadPart = 0;
            Irp->IoStatus.Information = sizeof(FILE_POSITION_INFORMATION);
        }

    } else {
        status = STATUS_INVALID_PARAMETER;
    }

    Irp->IoStatus.Status = status;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "--SerialQueryInformationFile(%p, %p)=%Xh\r\n",
                Device,
                Irp,
                status);
    return status;
}

/*++

Routine Description:

    This routine is used to set the end of file information on
    the opened parallel port.  Any other file information request
    is retured with an invalid parameter.

    This routine always ignores the actual end of file since
    the query information code always returns an end of file of 0.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

The function value is the final status of the call

--*/
_Use_decl_annotations_
NTSTATUS
SerialSetInformationFile(
    WDFDEVICE Device,
    PIRP Irp
    )
{
    NTSTATUS status;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "++SerialSetInformationFile(%p, %p)\r\n",
                Device,
                Irp);

    Irp->IoStatus.Information = 0L;

    if ((IoGetCurrentIrpStackLocation(Irp)->
            Parameters.SetFile.FileInformationClass ==
         FileEndOfFileInformation) ||
        (IoGetCurrentIrpStackLocation(Irp)->
            Parameters.SetFile.FileInformationClass ==
         FileAllocationInformation)) {

        status = STATUS_SUCCESS;

    } else {

        status = STATUS_INVALID_PARAMETER;

    }

    Irp->IoStatus.Status = status;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "--SerialSetInformationFile(%p, %p)=%Xh\r\n",
                Device,
                Irp,
                status);
    return status;
}

