/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:
    Definition of wavert miniport class.

--*/

#pragma once

//=============================================================================
// Referenced Forward
//=============================================================================
class CMiniportWaveRTStream;
typedef CMiniportWaveRTStream *PCMiniportWaveRTStream;

//=============================================================================
// Classes
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveRT
//   
class CMiniportWaveRT : 
    public IMiniportWaveRT,
    public CUnknown
{
private:
    ULONG                               m_ulSystemAllocated;
    DWORD                               m_dwSystemAllocatedModes;
    ULONG                               m_ulMaxSystemStreams;
    PPORTWAVERT                         m_Port; // Port driver object.

    // weak ref of running streams.
    PCMiniportWaveRTStream            * m_SystemStreams;
    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE  m_pDeviceFormat;
    PCFILTER_DESCRIPTOR                 m_FilterDesc;
    PIN_DEVICE_FORMATS_AND_MODES *      m_DeviceFormatsAndModes;
    ULONG                               m_DeviceFormatsAndModesCount; 
    USHORT                              m_DeviceMaxChannels;

    union {
        PVOID                           m_DeviceContext;
    };
    PDEVICE_OBJECT		                m_PwmDevice;

protected:
    PADAPTERCOMMON                      m_pAdapterCommon;
    ULONG                               m_DeviceFlags;
    eDeviceType                         m_DeviceType;
    PPORTEVENTS                         m_pPortEvents;
    PENDPOINT_MINIPAIR                  m_pMiniportPair;

public:

    DECLARE_PROPERTYHANDLER(Get_SoundDetectorSupportedPatterns);
    DECLARE_PROPERTYHANDLER(Get_SoundDetectorPatterns);
    DECLARE_PROPERTYHANDLER(Set_SoundDetectorPatterns);
    DECLARE_PROPERTYHANDLER(Get_SoundDetectorArmed);
    DECLARE_PROPERTYHANDLER(Set_SoundDetectorArmed);
    DECLARE_PROPERTYHANDLER(Get_SoundDetectorMatchResult);

    NTSTATUS EventHandler_SoundDetectorMatchDetected
    (
        _In_  PPCEVENT_REQUEST EventRequest
    );

    NTSTATUS ValidateStreamCreate
    (
        _In_ ULONG _Pin, 
        _In_ BOOLEAN _Capture,
        _In_ GUID _SignalProcessingMode
    );
    
    NTSTATUS StreamCreated
    (
        _In_ ULONG                  _Pin,
        _In_ PCMiniportWaveRTStream _Stream
    );
    
    NTSTATUS StreamClosed
    (
        _In_ ULONG _Pin,
        _In_ PCMiniportWaveRTStream _Stream
    );
    
    NTSTATUS IsFormatSupported
    ( 
        _In_ ULONG          _ulPin, 
        _In_ BOOLEAN        _bCapture,
        _In_ PKSDATAFORMAT  _pDataFormat
    );

    static NTSTATUS GetAttributesFromAttributeList
    (
        _In_ const KSMULTIPLE_ITEM *_pAttributes,
        _In_ size_t _Size,
        _Out_ GUID* _pSignalProcessingMode
    );

public:
    DECLARE_STD_UNKNOWN();

#pragma code_seg("PAGE")
    CMiniportWaveRT(
        _In_            PUNKNOWN                                UnknownAdapter,
        _In_            PENDPOINT_MINIPAIR                      MiniportPair,
        _In_opt_        PVOID                                   DeviceContext
        )
        :CUnknown(0),
        m_ulMaxSystemStreams(0),
        m_DeviceType(MiniportPair->DeviceType),
        m_DeviceContext(DeviceContext), 
        m_DeviceMaxChannels(MiniportPair->DeviceMaxChannels),
        m_DeviceFormatsAndModes(MiniportPair->PinDeviceFormatsAndModes),
        m_DeviceFormatsAndModesCount(MiniportPair->PinDeviceFormatsAndModesCount),
        m_DeviceFlags(MiniportPair->DeviceFlags),
        m_pMiniportPair(MiniportPair)
    {
        PAGED_CODE();

        m_pAdapterCommon = (PADAPTERCOMMON)UnknownAdapter; // weak ref.
        
        if (MiniportPair->WaveDescriptor)
        {
            RtlCopyMemory(&m_FilterDesc, MiniportPair->WaveDescriptor, sizeof(m_FilterDesc));
            
            //
            // Get the max # of pin instances.
            //
            if (IsRenderDevice())
            {
                if (m_FilterDesc.PinCount > KSPIN_WAVE_RENDER_SOURCE)
                    {
                        m_ulMaxSystemStreams = m_FilterDesc.Pins[KSPIN_WAVE_RENDER_SINK_SYSTEM].MaxFilterInstanceCount;
                    }
            }
        }
        
    }

#pragma code_seg()

    ~CMiniportWaveRT();

    IMP_IMiniportWaveRT;
    IMP_IMiniportAudioEngineNode;

    STDMETHODIMP_(NTSTATUS) GetModes
		(
		_In_                                        ULONG   Pin, 
		_Out_writes_opt_(*NumSignalProcessingModes) GUID*   SignalProcessingModes, 
		_Inout_                                     ULONG*  NumSignalProcessingModes
		);

    // Friends
    friend class        CMiniportWaveRTStream;
    friend class        CMiniportTopologySimple;
    
    friend NTSTATUS PropertyHandler_WaveFilter
    (   
        _In_ PPCPROPERTY_REQUEST      PropertyRequest 
    );   

public:

    NTSTATUS PropertyHandlerProposedFormat
    (
        _In_ PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS PropertyHandlerProposedFormat2
    (
        _In_ PPCPROPERTY_REQUEST PropertyRequest
    );

    PADAPTERCOMMON GetAdapterCommObj() 
    {
        return m_pAdapterCommon; 
    };
#pragma code_seg()

private:
#pragma code_seg("PAGE")
    //---------------------------------------------------------------------------
    // GetPinSupportedDeviceFormats 
    //
    //  Return supported formats for a given pin.
    //
    //  Return value
    //      The number of KSDATAFORMAT_WAVEFORMATEXTENSIBLE items.
    //
    //  Remarks
    //      Supported formats index array follows same order as filter's pin
    //      descriptor list.
    //
    _Post_satisfies_(return > 0)
    ULONG GetPinSupportedDeviceFormats(_In_ ULONG PinId, _Outptr_opt_result_buffer_(return) KSDATAFORMAT_WAVEFORMATEXTENSIBLE **ppFormats)
    {
        PAGED_CODE();

        ASSERT(m_DeviceFormatsAndModesCount > PinId);
        ASSERT(m_DeviceFormatsAndModes[PinId].WaveFormats != NULL);
        ASSERT(m_DeviceFormatsAndModes[PinId].WaveFormatsCount > 0);

        if (ppFormats != NULL)
        {
            *ppFormats = m_DeviceFormatsAndModes[PinId].WaveFormats;
        }
        
        return m_DeviceFormatsAndModes[PinId].WaveFormatsCount;
    }

    //---------------------------------------------------------------------------
    // GetAudioEngineSupportedDeviceFormats 
    //
    //  Return supported device formats for the audio engine node.
    //
    //  Return value
    //      The number of KSDATAFORMAT_WAVEFORMATEXTENSIBLE items.
    //
    //  Remarks
    //      Supported formats index array follows same order as filter's pin
    //      descriptor list. This routine assumes the engine formats are the
    //      last item in the filter's array of PIN_DEVICE_FORMATS_AND_MODES.
    //
    _Post_satisfies_(return > 0)
    ULONG GetAudioEngineSupportedDeviceFormats(_Outptr_opt_result_buffer_(return) KSDATAFORMAT_WAVEFORMATEXTENSIBLE **ppFormats)
    {
        PAGED_CODE();

        ULONG i;

        // By convention, the audio engine node's device formats are the last
        // entry in the PIN_DEVICE_FORMATS_AND_MODES list.
        
        // Since this endpoint apparently supports offload, there must be at least a system,
        // offload, and loopback pin, plus the entry for the device formats.
        ASSERT(m_DeviceFormatsAndModesCount > 3);

        i = m_DeviceFormatsAndModesCount - 1;                       // Index of last list entry

        ASSERT(m_DeviceFormatsAndModes[i].PinType == NoPin);
        ASSERT(m_DeviceFormatsAndModes[i].WaveFormats != NULL);
        ASSERT(m_DeviceFormatsAndModes[i].WaveFormatsCount > 0);

        if (ppFormats != NULL)
        {
            *ppFormats = m_DeviceFormatsAndModes[i].WaveFormats;
        }

        return m_DeviceFormatsAndModes[i].WaveFormatsCount;
    }

    //---------------------------------------------------------------------------
    // GetPinSupportedDeviceModes 
    //
    //  Return mode information for a given pin.
    //
    //  Return value
    //      The number of MODE_AND_DEFAULT_FORMAT items or 0 if none.
    //
    //  Remarks
    //      Supported formats index array follows same order as filter's pin
    //      descriptor list.
    //
    _Success_(return != 0)
    ULONG GetPinSupportedDeviceModes(_In_ ULONG PinId, _Outptr_opt_result_buffer_(return) _On_failure_(_Deref_post_null_) MODE_AND_DEFAULT_FORMAT **ppModes)
    {
        PAGED_CODE();

        PMODE_AND_DEFAULT_FORMAT modes;
        ULONG numModes;

        ASSERT(m_DeviceFormatsAndModesCount > PinId);
        ASSERT((m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormatCount == 0) == (m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormat == NULL));

        modes = m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormat;
        numModes = m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormatCount;

        if (ppModes != NULL)
        {
            if (numModes > 0)
            {
                *ppModes = modes;
            }
            else
            {
                // ensure that the returned pointer is NULL
                // in the event of failure (SAL annotation above
                // indicates that it must be NULL, and OACR sees a possibility
                // that it might not be).
                *ppModes = NULL;
            }
        }

        return numModes;
    }
#pragma code_seg()

protected:
#pragma code_seg("PAGE")
    BOOL IsRenderDevice()
    {
        PAGED_CODE();

        return (m_DeviceType == eSpeakerHpDevice) ? TRUE : FALSE;
    }

    BOOL IsSystemRenderPin(ULONG nPinId)
    {
        PAGED_CODE();

        PINTYPE pinType = m_DeviceFormatsAndModes[nPinId].PinType;
        return (pinType == SystemRenderPin);
    }

    BOOL IsBridgePin(ULONG nPinId)
    {
        PAGED_CODE();

        return (m_DeviceFormatsAndModes[nPinId].PinType == BridgePin);
    }

    ULONG GetSystemPinId()
    {
        PAGED_CODE();

        ASSERT(IsRenderDevice());
        return KSPIN_WAVE_RENDER_SINK_SYSTEM;
    }

#pragma code_seg()

};
typedef CMiniportWaveRT *PCMiniportWaveRT;


