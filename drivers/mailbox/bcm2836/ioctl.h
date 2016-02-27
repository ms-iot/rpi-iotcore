//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    ioctl.h
//
// Abstract:
//
//    This file contains the ioctl header.
//

#pragma once

EXTERN_C_START

#define IOCTL_MAILBOX_VCHIQ_INPUT_BUFFER_SIZE 8

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL RpiqProcessChannel;

EXTERN_C_END