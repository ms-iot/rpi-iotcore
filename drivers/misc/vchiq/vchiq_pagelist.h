//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     vchiq_pagelist.h
//
// Abstract:
//
//     This header contains sepcific definition from Raspberry Pi
//     public userland repo. This is based on the vchiq_pagelist.h header.
//     Only minimal definitions are ported over.
//

#pragma once

EXTERN_C_START

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define CACHE_LINE_SIZE 32
#define PAGELIST_WRITE 0
#define PAGELIST_READ 1
#define PAGELIST_READ_WITH_FRAGMENTS 2

#include <pshpack1.h>

typedef struct _VCHIQ_PAGELIST {
    ULONG Length;
    USHORT Type;
    USHORT Offset;
    ULONG Addrs[1];    /* N.B. 12 LSBs hold the number of following
                       pages at consecutive addresses. */
} VCHIQ_PAGELIST, *PVCHIQ_PAGELIST;

typedef struct _FRAGMENTS {
    char Headbuf[CACHE_LINE_SIZE];
    char Tailbuf[CACHE_LINE_SIZE];
} FRAGMENTS, *PFRAGMENTS;

#include <poppack.h>

EXTERN_C_END