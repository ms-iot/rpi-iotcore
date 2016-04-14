//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//      trace.c
//
// Abstract:
//
//      Vchiq trace related implementation
//

#include "precomp.h"

ULONG VchiqCriticalDebugBreak (
    )
{
    for (;;) {
        char response[2] = { 0 };
        DbgPrompt(
            "Break to debug (b) or ignore (i)? ",
            response,
            sizeof(response));

        if ((response[0] == 'B') || (response[0] == 'b')) {
            DbgBreakPoint();
            break;
        } else if ((response[0] == 'I') || (response[0] == 'i')) {
            break;
        }
    }

    return 1;
}