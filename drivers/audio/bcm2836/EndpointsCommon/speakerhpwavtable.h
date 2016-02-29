/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:
    Declaration of wave miniport tables for the speaker (external: headphone).

--*/

#pragma once

// To keep the code simple assume device supports only 48KHz, 16-bit, stereo (PCM and NON-PCM)

#define SPEAKERHP_DEVICE_MAX_CHANNELS                   2       // Max Channels.

#define SPEAKERHP_HOST_MAX_CHANNELS                     2       // Max Channels.
#define SPEAKERHP_HOST_MIN_BITS_PER_SAMPLE              16      // Min Bits Per Sample
#define SPEAKERHP_HOST_MAX_BITS_PER_SAMPLE              16      // Max Bits Per Sample
#define SPEAKERHP_HOST_MIN_SAMPLE_RATE                  44100   // Min Sample Rate
#define SPEAKERHP_HOST_MAX_SAMPLE_RATE                  44100   // Max Sample Rate

//
// Max # of pin instances.
//
#define SPEAKERHP_MAX_INPUT_SYSTEM_STREAMS              2       // Raw + Default streams

static 
KSDATAFORMAT_WAVEFORMATEXTENSIBLE SpeakerHpHostPinSupportedDeviceFormats[] =
{
    { // 2
        {
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,
                44100,
                176400,
                4,
                16,
                sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX)
            },
            16,
            KSAUDIO_SPEAKER_STEREO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    },
};

//
// Supported modes (only on streaming pins).
//
static
MODE_AND_DEFAULT_FORMAT SpeakerHpHostPinSupportedDeviceModes[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW,
        &SpeakerHpHostPinSupportedDeviceFormats[0].DataFormat // 44.1kHz
    },
};

//
// The entries here must follow the same order as the filter's pin
// descriptor array.
//
static 
PIN_DEVICE_FORMATS_AND_MODES SpeakerHpPinDeviceFormatsAndModes[] = 
{
    {
        SystemRenderPin,
        SpeakerHpHostPinSupportedDeviceFormats,
        SIZEOF_ARRAY(SpeakerHpHostPinSupportedDeviceFormats),
        SpeakerHpHostPinSupportedDeviceModes,
        SIZEOF_ARRAY(SpeakerHpHostPinSupportedDeviceModes)
    },
    {
        BridgePin,
        NULL,
        0,
        NULL,
        0
    },
};

//=============================================================================
static
KSDATARANGE_AUDIO SpeakerHpPinDataRangesStream[] =
{
    { // 0
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        SPEAKERHP_HOST_MAX_CHANNELS,           
        SPEAKERHP_HOST_MIN_BITS_PER_SAMPLE,    
        SPEAKERHP_HOST_MAX_BITS_PER_SAMPLE,    
        SPEAKERHP_HOST_MIN_SAMPLE_RATE,            
        SPEAKERHP_HOST_MAX_SAMPLE_RATE             
    },
};


static
PKSDATARANGE SpeakerHpPinDataRangePointersStream[] =
{
    PKSDATARANGE(&SpeakerHpPinDataRangesStream[0]),
    PKSDATARANGE(&PinDataRangeAttributeList)
};

//=============================================================================
static
KSDATARANGE SpeakerHpPinDataRangesBridge[] =
{
    {
        sizeof(KSDATARANGE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    }
};

static
PKSDATARANGE SpeakerHpPinDataRangePointersBridge[] =
{
    &SpeakerHpPinDataRangesBridge[0]
};

//=============================================================================

//=============================================================================
static
PCPIN_DESCRIPTOR SpeakerHpWaveMiniportPins[] =
{
    // Wave Out Streaming Pin (renderer) KSPIN_WAVE_RENDER_SINK_SYSTEM
    {
        SPEAKERHP_MAX_INPUT_SYSTEM_STREAMS,
        SPEAKERHP_MAX_INPUT_SYSTEM_STREAMS, 
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(SpeakerHpPinDataRangePointersStream),
            SpeakerHpPinDataRangePointersStream,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Wave Out Bridge Pin (Renderer) KSPIN_WAVE_RENDER_SOURCE
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(SpeakerHpPinDataRangePointersBridge),
            SpeakerHpPinDataRangePointersBridge,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
};

//=============================================================================
static
PCNODE_DESCRIPTOR SpeakerHpWaveMiniportNodes[] =
{
    // KSNODE_WAVE_AUDIO_ENGINE
    {
        0,                          // Flags
        NULL,                       // AutomationTable
        &KSNODETYPE_AUDIO_ENGINE,   // Type  KSNODETYPE_AUDIO_ENGINE
        NULL                        // Name
    }
};
//=============================================================================
//
//                   ----------------------------      
//                   |                          |      
//  System Pin   0-->|                          |--> 1 KSPIN_WAVE_RENDER_SOURCE
//                   |   HW Audio Engine node   |      
//                   |                          | 
//                   |                          |      
//                   ----------------------------       
static
PCCONNECTION_DESCRIPTOR SpeakerHpWaveMiniportConnections[] =
{
    { PCFILTER_NODE,            KSPIN_WAVE_RENDER_SINK_SYSTEM,     KSNODE_WAVE_AUDIO_ENGINE,   1 },
    { KSNODE_WAVE_AUDIO_ENGINE, 0,                                 PCFILTER_NODE,              KSPIN_WAVE_RENDER_SOURCE },
};

//=============================================================================
static
PCPROPERTY_ITEM PropertiesSpeakerHpWaveFilter[] =
{
    {
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_PROPOSEDATAFORMAT,
        KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_WaveFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationSpeakerHpWaveFilter, PropertiesSpeakerHpWaveFilter);

//=============================================================================
static
PCFILTER_DESCRIPTOR SpeakerHpWaveMiniportFilterDescriptor =
{
    0,                                              // Version
    &AutomationSpeakerHpWaveFilter,                 // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                       // PinSize
    SIZEOF_ARRAY(SpeakerHpWaveMiniportPins),        // PinCount
    SpeakerHpWaveMiniportPins,                      // Pins
    sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
    SIZEOF_ARRAY(SpeakerHpWaveMiniportNodes),       // NodeCount
    SpeakerHpWaveMiniportNodes,                     // Nodes
    SIZEOF_ARRAY(SpeakerHpWaveMiniportConnections), // ConnectionCount
    SpeakerHpWaveMiniportConnections,               // Connections
    0,                                              // CategoryCount
    NULL                                            // Categories  - use defaults (audio, render, capture)
};


