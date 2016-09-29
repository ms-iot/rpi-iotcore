// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    wmi.c
//
// Abstract:
//
//    This module contains the code that handles the wmi IRPs for mini Uart
//    serial driver.

#include "precomp.h"
#include <wmistr.h>
#include "wmi.tmh"

EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiQueryPortName;
EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiQueryPortCommData;
EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiQueryPortHWData;
EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiQueryPortPerfData;
EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiQueryPortPropData;

NTSTATUS
SerialWmiRegisterInstance(
    _In_ WDFDEVICE Device,
    _In_ const GUID* Guid,
    _In_ ULONG MinInstanceBufferSize,
    _In_ PFN_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGESRP0, SerialWmiRegistration)
#pragma alloc_text(PAGESRP0, SerialWmiRegisterInstance)
#pragma alloc_text(PAGESRP0, EvtWmiQueryPortName)
#pragma alloc_text(PAGESRP0, EvtWmiQueryPortCommData)
#pragma alloc_text(PAGESRP0, EvtWmiQueryPortHWData)
#pragma alloc_text(PAGESRP0, EvtWmiQueryPortPerfData)
#pragma alloc_text(PAGESRP0, EvtWmiQueryPortPropData)
#endif

_Use_decl_annotations_
NTSTATUS
SerialWmiRegisterInstance(
    WDFDEVICE Device,
    const GUID* Guid,
    ULONG MinInstanceBufferSize,
    PFN_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance
    )
{
    NTSTATUS status;
    WDF_WMI_PROVIDER_CONFIG providerConfig;
    WDF_WMI_INSTANCE_CONFIG instanceConfig;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "++SerialWmiRegisterInstance\r\n");

    // Create and register WMI providers and instances  blocks

    WDF_WMI_PROVIDER_CONFIG_INIT(&providerConfig, Guid);
    providerConfig.MinInstanceBufferSize = MinInstanceBufferSize;

    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instanceConfig, &providerConfig);
    instanceConfig.Register = TRUE;
    instanceConfig.EvtWmiInstanceQueryInstance = EvtWmiInstanceQueryInstance;

    status = WdfWmiInstanceCreate(Device,
                                  &instanceConfig,
                                  WDF_NO_OBJECT_ATTRIBUTES,
                                  WDF_NO_HANDLE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "--SerialWmiRegisterInstance()=%Xh\r\n",status);
    return status;
}

/*++

Routine Description:

    Registers with WMI as a data provider for this
    instance of the device

Arguments:

    Device - Handle to the mini Uart device

Return Value:
    Status

--*/
_Use_decl_annotations_
NTSTATUS
SerialWmiRegistration(
    WDFDEVICE Device
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PSERIAL_DEVICE_EXTENSION devExt;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "++SerialWmiRegistration\r\n");

    devExt = SerialGetDeviceExtension (Device);

    // Fill in wmi perf data (all zero's)

    RtlZeroMemory(&devExt->WmiPerfData, sizeof(devExt->WmiPerfData));

    status = SerialWmiRegisterInstance(Device,
                                       &MSSerial_PortName_GUID,
                                       0,
                                       EvtWmiQueryPortName);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WMI,
                    "SerialWmiRegistration() SerialWmiRegisterInstance(serGUID) failed. Err=%Xh\r\n",
                    status);
        goto EndWmiReg;
    }

    status = SerialWmiRegisterInstance(Device,
                                       &MSSerial_CommInfo_GUID,
                                       sizeof(SERIAL_WMI_COMM_DATA),
                                       EvtWmiQueryPortCommData);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WMI,
                    "SerialWmiRegistration() SerialWmiRegisterInstance(WMICOMDA) failed. Err=%Xh\r\n",
                    status);
        goto EndWmiReg;
    }

    status = SerialWmiRegisterInstance(Device,
                                       &MSSerial_HardwareConfiguration_GUID,
                                       sizeof(SERIAL_WMI_HW_DATA),
                                       EvtWmiQueryPortHWData);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WMI,
                    "SerialWmiRegistration() SerialWmiRegisterInstance(WMIHWDAT) failed. Err=%Xh\r\n",
                    status);
        return status;
    }

    status = SerialWmiRegisterInstance(Device,
                                       &MSSerial_PerformanceInformation_GUID,
                                       sizeof(SERIAL_WMI_PERF_DATA),
                                       EvtWmiQueryPortPerfData);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WMI,
                    "SerialWmiRegistration() SerialWmiRegisterInstance(WMIPERF) failed. Err=%Xh\r\n",
                    status);
        goto EndWmiReg;
    }

    status = SerialWmiRegisterInstance(Device,
                                       &MSSerial_CommProperties_GUID,
                                       sizeof(SERIAL_COMMPROP) + sizeof(ULONG),
                                       EvtWmiQueryPortPropData);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WMI,
                    "SerialWmiRegistration() SerialWmiRegisterInstance(COMMPRPOP) failed. Err=%Xh\r\n",
                    status);
    }

EndWmiReg:
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI,
                "--SerialWmiRegistration()=%Xh\r\n",
                status);
    return status;
}

/*++

Routine Description:

    WMI Call back functions

Arguments:

    WmiInstance
    OutBufferSize
    OutBuffer
    BufferUsed

Return Value:
    Status

--*/
_Use_decl_annotations_
NTSTATUS
EvtWmiQueryPortName(
    WDFWMIINSTANCE WmiInstance,
    ULONG OutBufferSize,
    PVOID OutBuffer,
    PULONG BufferUsed
    )
{
    WDFDEVICE device;
    WCHAR regName[SYMBOLIC_NAME_LENGTH];
    UNICODE_STRING string;
    USHORT nameSize = sizeof(regName);
    NTSTATUS status;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "++EvtWmiQueryPortName()\r\n");

    device = WdfWmiInstanceGetDevice(WmiInstance);

    status = SerialReadSymName(device, regName, &nameSize);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WMI,
                    "EvtWmiQueryPortName() SerialReadSymName failed. Err=%Xh\r\n",
                    status);
        goto EndWmiPortname;
    }

    RtlInitUnicodeString(&string, regName);

    status = WDF_WMI_BUFFER_APPEND_STRING(OutBuffer,
                                        OutBufferSize,
                                        &string,
                                        BufferUsed);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WMI,
                    "EvtWmiQueryPortName() WMI_BUFFER_APPEND_STRING failed. Err=%Xh\r\n",
                    status);
    }

EndWmiPortname:

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "--EvtWmiQueryPortName()=%Xh\r\n", status);
    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtWmiQueryPortCommData(
    WDFWMIINSTANCE WmiInstance,
    ULONG  OutBufferSize,
    PVOID  OutBuffer,
    PULONG BufferUsed
    )
{
    PSERIAL_DEVICE_EXTENSION devExt;
    UNREFERENCED_PARAMETER(OutBufferSize);
    NTSTATUS status;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "++EvtWmiQueryPortCommData()\r\n");

    devExt = SerialGetDeviceExtension (WdfWmiInstanceGetDevice(WmiInstance));

    *BufferUsed = sizeof(SERIAL_WMI_COMM_DATA);

    if (OutBufferSize < *BufferUsed) {

        status=STATUS_INSUFFICIENT_RESOURCES;

    } else {

        *(PSERIAL_WMI_COMM_DATA)OutBuffer = devExt->WmiCommData;
        status=STATUS_SUCCESS;
    };
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI,
                "--EvtWmiQueryPortCommData()=%Xh\r\n",
                status);
    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtWmiQueryPortHWData(
    WDFWMIINSTANCE WmiInstance,
    ULONG OutBufferSize,
    PVOID OutBuffer,
    PULONG BufferUsed
    )
{
    PSERIAL_DEVICE_EXTENSION devExt;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(OutBufferSize);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "++EvtWmiQueryPortHWData()\r\n");

    devExt = SerialGetDeviceExtension(WdfWmiInstanceGetDevice(WmiInstance));

    *BufferUsed = sizeof(SERIAL_WMI_HW_DATA);

    if (OutBufferSize < *BufferUsed) {
        
        status = STATUS_INSUFFICIENT_RESOURCES;

    } else {
    
        *(PSERIAL_WMI_HW_DATA)OutBuffer = devExt->WmiHwData;
        status = STATUS_SUCCESS;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI,
            "--EvtWmiQueryPortHWData()=%Xh\r\n",
            status);
    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtWmiQueryPortPerfData(
    WDFWMIINSTANCE WmiInstance,
    ULONG OutBufferSize,
    PVOID OutBuffer,
    PULONG BufferUsed
    )
{
    PSERIAL_DEVICE_EXTENSION devExt;
    UNREFERENCED_PARAMETER(OutBufferSize);
    NTSTATUS status;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "++EvtWmiQueryPortPerfData()\r\n");

    devExt = SerialGetDeviceExtension(WdfWmiInstanceGetDevice(WmiInstance));

    *BufferUsed = sizeof(SERIAL_WMI_PERF_DATA);

    if (OutBufferSize < *BufferUsed) {

        status = STATUS_INSUFFICIENT_RESOURCES;

    } else {

        *(PSERIAL_WMI_PERF_DATA)OutBuffer = devExt->WmiPerfData;
        status = STATUS_SUCCESS;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "--EvtWmiQueryPortPerfData()=%Xh\r\n", status);
    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtWmiQueryPortPropData(
    WDFWMIINSTANCE WmiInstance,
    ULONG OutBufferSize,
    PVOID OutBuffer,
    PULONG BufferUsed
    )
{
    PSERIAL_DEVICE_EXTENSION devExt;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(OutBufferSize);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "++EvtWmiQueryPortPerfData()\r\n");

    devExt = SerialGetDeviceExtension(WdfWmiInstanceGetDevice(WmiInstance));

    *BufferUsed = sizeof(SERIAL_COMMPROP) + sizeof(ULONG);

    if (OutBufferSize < *BufferUsed) {

        status = STATUS_INSUFFICIENT_RESOURCES;

    } else {

        SerialGetProperties(devExt,
                            (PSERIAL_COMMPROP)OutBuffer);

        *((PULONG)(((PSERIAL_COMMPROP)OutBuffer)->ProvChar)) = 0;
        status = STATUS_SUCCESS;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WMI, "--EvtWmiQueryPortPerfData()=%Xh\r\n", status);
    return status;
}

