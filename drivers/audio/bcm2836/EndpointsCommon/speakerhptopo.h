
/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:
    Declaration of topology miniport for the speaker (external: headphone).

--*/

#pragma once

NTSTATUS 
PropertyHandler_SpeakerHpTopoFilter
(
    _In_ PPCPROPERTY_REQUEST PropertyRequest
);

