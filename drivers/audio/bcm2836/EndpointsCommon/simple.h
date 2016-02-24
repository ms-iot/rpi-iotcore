/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:
    Node and Pin numbers and other common definitions for rpisimple configuration.

--*/

#pragma once

// Name Guid
// {F6730547-F795-4FEC-ACF4-17344717375F}
#define STATIC_NAME_RPIWAV_SIMPLE\
    0xf6730547, 0xf795, 0x4fec, 0xac, 0xf4, 0x17, 0x34, 0x47, 0x17, 0x37, 0x5f
DEFINE_GUIDSTRUCT("F6730547-F795-4FEC-ACF4-17344717375F", NAME_RPIWAV_SIMPLE);
#define NAME_RPIWAV_SIMPLE DEFINE_GUIDNAMED(NAME_RPIWAV_SIMPLE)

//----------------------------------------------------
// New defines for the render endpoints.
//----------------------------------------------------

// Default pin instances.
#define MAX_INPUT_SYSTEM_STREAMS        1

// Wave Topology nodes
enum
{
    KSNODE_WAVE_AUDIO_ENGINE = 0
};

// Wave pins
enum
{
    KSPIN_WAVE_RENDER_SINK_SYSTEM = 0,
    KSPIN_WAVE_RENDER_SOURCE
};

// Topology pins.
enum
{
    KSPIN_TOPO_WAVEOUT_SOURCE = 0,
    KSPIN_TOPO_LINEOUT_DEST,
};

// Wave Topology nodes.
enum 
{
    KSNODE_WAVE_ADC = 0
};

// data format attribute range definitions.
static
KSATTRIBUTE PinDataRangeSignalProcessingModeAttribute =
{
    sizeof(KSATTRIBUTE),
    0,
    STATICGUIDOF(KSATTRIBUTEID_AUDIOSIGNALPROCESSING_MODE),
};

static
PKSATTRIBUTE PinDataRangeAttributes[] =
{
    &PinDataRangeSignalProcessingModeAttribute,
};

static
KSATTRIBUTE_LIST PinDataRangeAttributeList =
{
    ARRAYSIZE(PinDataRangeAttributes),
    PinDataRangeAttributes,
};


