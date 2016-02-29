/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:
    Common declaration of topology table for the headphone.

--*/

#pragma once

//=============================================================================
static
KSJACK_DESCRIPTION HpJackDesc =
{
    KSAUDIO_SPEAKER_STEREO,
    JACKDESC_RGB(179, 201, 140),
    eConnTypeOtherAnalog,
    eGeoLocTop,
    eGenLocPrimaryBox,
    ePortConnJack,
    TRUE
};
