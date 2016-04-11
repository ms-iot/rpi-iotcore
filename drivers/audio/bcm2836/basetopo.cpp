/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:
    Implementation of topology miniport.

--*/

//4127: conditional expression is constant
//4595: illegal inline operator
#pragma warning (disable : 4127)
#pragma warning (disable : 4595)

#include <rpiwav.h>
#include "basetopo.h"

//=============================================================================
#pragma code_seg("PAGE")
CMiniportTopologyRPIWAV::CMiniportTopologyRPIWAV
(
    _In_ PCFILTER_DESCRIPTOR    *FilterDesc,
    _In_ USHORT                 DeviceMaxChannels
)
/*++

Routine Description:

  Topology miniport constructor

Arguments:

  FilterDesc - filter description

  DeviceMaxChannels - max channels of the device

Return Value:

  None

--*/
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    m_AdapterCommon     = NULL;

    ASSERT(FilterDesc != NULL);
    m_FilterDescriptor  = FilterDesc;
    m_PortEvents        = NULL;
    
    ASSERT(DeviceMaxChannels > 0);
    m_DeviceMaxChannels = DeviceMaxChannels;
} 

  //=============================================================================
#pragma code_seg("PAGE")
CMiniportTopologyRPIWAV::~CMiniportTopologyRPIWAV
(
    VOID
)
/*++

Routine Description:

  Topology miniport destructor

Arguments:

Return Value:

  void

--*/
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    SAFE_RELEASE(m_AdapterCommon);
    SAFE_RELEASE(m_PortEvents);
} 

//=============================================================================
#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
CMiniportTopologyRPIWAV::DataRangeIntersection
( 
    ULONG           PinId,
    PKSDATARANGE    ClientDataRange,
    PKSDATARANGE    MyDataRange,
    ULONG           OutputBufferLength,
    PVOID           ResultantFormat,
    PULONG          ResultantFormatLength 
)
/*++

Routine Description:

    The DataRangeIntersection function determines the highest 
    quality intersection of two data ranges. Topology miniport does nothing.

Arguments:

    PinId - Pin for which data intersection is being determined. 

    ClientDataRange - Pointer to KSDATARANGE structure which contains the data range 
                      submitted by client in the data range intersection property 
                      request

    MyDataRange - Pin's data range to be compared with client's data range

    OutputBufferLength - Size of the buffer pointed to by the resultant format parameter

    ResultantFormat - Pointer to value where the resultant format should be returned

    ResultantFormatLength - Actual length of the resultant format that is placed 
                            at ResultantFormat. This should be less than or equal 
                            to OutputBufferLength

Return Value:

    NT status code

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(ClientDataRange);
    UNREFERENCED_PARAMETER(MyDataRange);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ResultantFormat);
    UNREFERENCED_PARAMETER(ResultantFormatLength);

    DPF_ENTER(("[%s]",__FUNCTION__));

    return (STATUS_NOT_IMPLEMENTED);
} 

//=============================================================================
#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
CMiniportTopologyRPIWAV::GetDescription
( 
    PPCFILTER_DESCRIPTOR *  OutFilterDescriptor 
)
/*++

Routine Description:

    The GetDescription function gets a pointer to a filter description. 
    It provides a location to deposit a pointer in miniport's description 
    structure. This is the placeholder for the FromNode or ToNode fields in 
    connections which describe connections to the filter's pins

Arguments:

    OutFilterDescriptor - Pointer to the filter description. 

Return Value:

    NT status code

--*/
{
    PAGED_CODE();

    ASSERT(OutFilterDescriptor);

    DPF_ENTER(("[%s]",__FUNCTION__));

    *OutFilterDescriptor = m_FilterDescriptor;

    return (STATUS_SUCCESS);
} 

//=============================================================================
#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
CMiniportTopologyRPIWAV::Init
( 
    PUNKNOWN          UnknownAdapter,
    PPORTTOPOLOGY     Port
)
/*++

Routine Description:

    Initializes the topology miniport.

Arguments:

    UnknownAdapter - adapter

    Port - topology port

Return Value:

    NT status code

--*/
{
    PAGED_CODE();
    
    ASSERT(UnknownAdapter);
    ASSERT(Port);

    DPF_ENTER(("[CMiniportTopologyRPIWAV::Init]"));

    NTSTATUS    ntStatus;

    ntStatus = UnknownAdapter->QueryInterface( 
                                    IID_IAdapterCommon,
                                    (PVOID *) &m_AdapterCommon);
    
    if (NT_SUCCESS(ntStatus))
    {
        //
        // Get the port event interface.
        //
        ntStatus = Port->QueryInterface(
                            IID_IPortEvents, 
                            (PVOID *)&m_PortEvents);
    }

    if (!NT_SUCCESS(ntStatus))
    {
        // clean up AdapterCommon
        SAFE_RELEASE(m_AdapterCommon);
        SAFE_RELEASE(m_PortEvents);
    }

    return ntStatus;
} 

//=============================================================================
#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
CMiniportTopologyRPIWAV::PropertyHandlerGeneric
(
    PPCPROPERTY_REQUEST     PropertyRequest
)
/*++

Routine Description:

    Handles all properties for this miniport.

Arguments:

    PropertyRequest - property request

Return Value:

    NT status code

--*/
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    switch (PropertyRequest->PropertyItem->Id)
    {
        case KSPROPERTY_AUDIO_CPU_RESOURCES:
            ntStatus = PropertyHandler_CpuResources(PropertyRequest);
            break;

        default:
            DPF(D_TERSE, ("[PropertyHandlerGeneric: Invalid Device Request]"));
    }

    return ntStatus;
} 

