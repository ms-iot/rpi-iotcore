/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Abstract:
    Helper functions for rpiwav

--*/

#pragma once

#include <portcls.h>
#include <ksdebug.h>

PWAVEFORMATEX                   
GetWaveFormatEx
(
    _In_  PKSDATAFORMAT           DataFormat
);

NTSTATUS                        
ValidatePropertyParams
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest, 
    _In_ ULONG                    ValueSize,
    _In_ ULONG                    InstanceSize = 0 
);

NTSTATUS
PropertyHandler_BasicSupport
(
    _In_  PPCPROPERTY_REQUEST     PropertyRequest,
    _In_  ULONG                   Flags,
    _In_  DWORD                   PropTypeSetId
);

NTSTATUS
PropertyHandler_CpuResources
( 
    _In_  PPCPROPERTY_REQUEST   PropertyRequest 
);

//=============================================================================
// Property helpers
//=============================================================================

NTSTATUS
RpiWavPropertyDispatch
(
    _In_ PPCPROPERTY_REQUEST PropertyRequest
);

// Use this structure to define property items with extra data allowing easier
// definition of separate get, set, and support handlers dispatched through
// RpiWavPropertyDispatch.
typedef struct
{
    PCPROPERTY_ITEM         PropertyItem;       // Standard PCPROPERTY_ITEM
    ULONG                   MinProperty;        // Minimum size of the property instance data
    ULONG                   MinData;            // Minimum size of the property value
    PCPFNPROPERTY_HANDLER   GetHandler;         // Property get handler (NULL if GET not supported)
    PCPFNPROPERTY_HANDLER   SetHandler;         // Property set handler (NULL if SET not supported)
    PCPFNPROPERTY_HANDLER   SupportHandler;     // Property support handler (NULL for common handler)
} RPIWAVPROPERTY_ITEM;

// The following macros facilitate adding property handlers to a class, allowing
// easier declaration and definition of a "thunk" routine that directly handles
// the property request and calls into a class instance method. Note that as
// written, the thunk routine assumes PAGED_CODE.
#define DECLARE_CLASSPROPERTYHANDLER(theClass, theMethod)                       \
NTSTATUS theClass##_##theMethod                                                 \
(                                                                               \
    _In_ PPCPROPERTY_REQUEST PropertyRequest                                    \
);

#define DECLARE_PROPERTYHANDLER(theMethod)                                      \
NTSTATUS theMethod                                                              \
(                                                                               \
    _In_ PPCPROPERTY_REQUEST PropertyRequest                                    \
);

#define DEFINE_CLASSPROPERTYHANDLER(theClass, theMethod)                        \
NTSTATUS theClass##_##theMethod                                                 \
(                                                                               \
    _In_ PPCPROPERTY_REQUEST PropertyRequest                                    \
)                                                                               \
{                                                                               \
    NTSTATUS status;                                                            \
                                                                                \
    PAGED_CODE();                                                               \
                                                                                \
    theClass* p = reinterpret_cast<theClass*>(PropertyRequest->MajorTarget);    \
                                                                                \
    p->AddRef();                                                                \
    status = p->theMethod(PropertyRequest);                                     \
    p->Release();                                                               \
                                                                                \
    return status;                                                              \
}                                                                               \
NTSTATUS theClass::theMethod                                                    \
(                                                                               \
    _In_ PPCPROPERTY_REQUEST PropertyRequest                                    \
)

