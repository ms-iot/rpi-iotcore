//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     precomp.h
//
// Abstract:
//
//  
//

#pragma once

#define INITGUID

// Window related header files
#include <ntddk.h>
#include <wdf.h>
#include <wdmguid.h>

// RPIQ driver public header
#include "rpiq.h"

// VCHIQ driver public header
#include "vchiq.h"

// VCHIQ ported over public header
#include "vchiq_common.h"
#include "vchiq_2835.h"
#include "vchiq_if.h"
#include "vchiq_cfg.h"
#include "vchiq_core.h"
#include "vchiq_pagelist.h"
#include "vchiq_ioctl.h"
