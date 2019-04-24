//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     RpiLanPropertyChange.cpp
//
// Abstract:
//
//     This file defines the exported functions for the DLL application.
//

#include "stdafx.h"

HINSTANCE g_hInstance = nullptr;

extern "C"
{
    // Externally callable functions
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        // Save the dll module handle
        g_hInstance = hModule;

        // Disable thread-attach calls
        DisableThreadLibraryCalls(hModule);
    }
    break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    break;

    case DLL_PROCESS_DETACH:
    {
        g_hInstance = NULL;
    }
    break;
    }
    return TRUE;
}

} // extern "C"

