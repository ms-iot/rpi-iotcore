/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:
    Local audio endpoint filter definitions. 

--*/

#pragma once

#include "speakerhptopo.h"
#include "speakerhptoptable.h"
#include "speakerhpwavtable.h"

NTSTATUS
CreateMiniportWaveRTRPIWAV
( 
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_TYPE,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

NTSTATUS
CreateMiniportTopologyRPIWAV
( 
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_TYPE,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

//
// Render miniports.
//
/*********************************************************************
* Topology/Wave bridge connection for speaker (external:headphone)   *
*                                                                    *
*              +------+                +------+                      *
*              | Wave |                | Topo |                      *
*              |      |                |      |                      *
* System   --->|0    1|--------------->|0    1|---> Line Out         *
*              |      |                |      |                      *
*              |      |                |      |                      *
*              +------+                +------+                      *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE SpeakerHpTopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_WAVEOUT_SOURCE,  // TopologyIn
        KSPIN_WAVE_RENDER_SOURCE,   // WaveOut
        CONNECTIONTYPE_WAVE_OUTPUT
    }
};

static
ENDPOINT_MINIPAIR SpeakerHpMiniports =
{
    eSpeakerHpDevice,
    L"TopologySpeakerHeadphone",            // make sure this name matches with KSNAME_TopologySpeakerHeadphone in the inf's [Strings] section 
    CreateMiniportTopologyRPIWAV,
    &SpeakerHpTopoMiniportFilterDescriptor,
    L"WaveSpeakerHeadphone",                // make sure this name matches with KSNAME_WaveSpeakerHeadphone in the inf's [Strings] section
    CreateMiniportWaveRTRPIWAV,
    &SpeakerHpWaveMiniportFilterDescriptor,
    SPEAKERHP_DEVICE_MAX_CHANNELS,
    SpeakerHpPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(SpeakerHpPinDeviceFormatsAndModes),
    SpeakerHpTopologyPhysicalConnections,
    SIZEOF_ARRAY(SpeakerHpTopologyPhysicalConnections),
    ENDPOINT_NO_FLAGS
};

//=============================================================================
//
// Render miniport pairs.
//
static
PENDPOINT_MINIPAIR  g_RenderEndpoints[] = 
{
    &SpeakerHpMiniports,
};

#define g_cRenderEndpoints  (SIZEOF_ARRAY(g_RenderEndpoints))

PENDPOINT_MINIPAIR  *g_CaptureEndpoints = nullptr;
#define g_cCaptureEndpoints (0)

//=============================================================================
//
// Total miniports = # endpoints * 2 (topology + wave).
//
#define g_MaxMiniports  ((g_cRenderEndpoints + g_cCaptureEndpoints) * 2)


