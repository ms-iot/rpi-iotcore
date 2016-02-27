//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    register.h
//
// Abstract:
//
//    This header contain mailbox related register definition
//

#pragma once

EXTERN_C_START

#define OFFSET_DIRECT_SDRAM         0xC0000000 // Direct Access to SDRAM
#define HEX_512_MB                  0x20000000
#define HEX_1_G                     0x40000000

// Status Bits
#define MAILBOX_STATUS_FULL         0x80000000
#define MAILBOX_STATUS_EMPTY        0x40000000
#define MAILBOX_FILL_LEVEL_MASK     0x0000000F

// Config Bits
#define MAILBOX_DATA_AVAIL_ENABLE_IRQ   0x00000001
#define MAILBOX_SPACE_AVAIL_ENABLE_IRQ  0x00000002
#define MAILBOX_OPP_EMPTY_ENABLE_IRQ    0x00000004
#define MAILBOX_RESET                   0x00000008
#define MAILBOX_DATA_AVAIL_PENDING      0x00000010
#define MAILBOX_SPACE_AVAIL_PENDING     0x00000020
#define MAILBOX_OPP_EMPTY_PENDING       0x00000040
#define MAILBOX_MASK_IRQ                0x00000007

// Mask
#define MAILBOX_CHANNEL_MASK        0x0000000F

// Power bit
#define POWER_SD                    0x0001
#define POWER_UART                  0x0002
#define POWER_MINIUART              0x0004
#define POWER_USB                   0x0008
#define POWER_12C0                  0x0010
#define POWER_12C1                  0x0020
#define POWER_12C2                  0x0040
#define POWER_SPI                   0x0080
#define POWER_CCP2TX                0x0100
#define POWER_DSI                   0x0200

enum MAILBOX_CHANNEL {
    MAILBOX_CHANNEL_POWER_MGMT      = 0,
    MAILBOX_CHANNEL_FB              = 1,
    MAILBOX_CHANNEL_VIRTUAL_UART    = 2,
    MAILBOX_CHANNEL_VCHIQ           = 3,
    MAILBOX_CHANNEL_LED             = 4,
    MAILBOX_CHANNEL_BUTTON          = 5,
    MAILBOX_CHANNEL_TOUCH_SCREEN    = 6,
    MAILBOX_CHANNEL_PROPERTY_ARM_VC = 8,
    MAILBOX_CHANNEL_PROPERTY_VC_ARM = 9,
    MAILBOX_CHANNEL_MAX             = 10
};

typedef struct _MAILBOX {
    volatile ULONG Read;
    volatile ULONG Rsvd0[3];
    volatile ULONG Poll;
    volatile ULONG Send;
    volatile ULONG Status;
    volatile ULONG Config;
    volatile ULONG Write;
} MAILBOX;

EXTERN_C_END
