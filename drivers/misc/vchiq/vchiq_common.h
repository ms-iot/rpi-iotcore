//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//     vchiq_common.h
//
// Abstract:
//
//     This header contains sepcific definition from Raspberry Pi
//     vchiq_common.h header in the public userland repo.
//     It also includes definition that is not included in a spcific
//     header file.
//

#pragma once

EXTERN_C_START

//callback reasons when an event occurs on a service
typedef enum _VCHI_CALLBACK_REASON {
    VCHI_CALLBACK_REASON_MIN,

    //This indicates that there is data available
    //handle is the msg id that was transmitted with the data
    //    When a message is received and there was no FULL message available previously, send callback
    //    Tasks get kicked by the callback, reset their event and try and read from the fifo until it fails
    VCHI_CALLBACK_MSG_AVAILABLE,
    VCHI_CALLBACK_MSG_SENT,
    VCHI_CALLBACK_MSG_SPACE_AVAILABLE, // XXX not yet implemented

                                       // This indicates that a transfer from the other side has completed
    VCHI_CALLBACK_BULK_RECEIVED,
    //This indicates that data queued up to be sent has now gone
    //handle is the msg id that was used when sending the data
    VCHI_CALLBACK_BULK_SENT,
    VCHI_CALLBACK_BULK_RX_SPACE_AVAILABLE, // XXX not yet implemented
    VCHI_CALLBACK_BULK_TX_SPACE_AVAILABLE, // XXX not yet implemented

    VCHI_CALLBACK_SERVICE_CLOSED,

    // this side has sent XOFF to peer due to lack of data consumption by service
    // (suggests the service may need to take some recovery action if it has
    // been deliberately holding off consuming data)
    VCHI_CALLBACK_SENT_XOFF,
    VCHI_CALLBACK_SENT_XON,

    // indicates that a bulk transfer has finished reading the source buffer
    VCHI_CALLBACK_BULK_DATA_READ,

    // power notification events (currently host side only)
    VCHI_CALLBACK_PEER_OFF,
    VCHI_CALLBACK_PEER_SUSPENDED,
    VCHI_CALLBACK_PEER_ON,
    VCHI_CALLBACK_PEER_RESUMED,
    VCHI_CALLBACK_FORCED_POWER_OFF,

#ifdef USE_VCHIQ_ARM
    // some extra notifications provided by vchiq_arm
    VCHI_CALLBACK_SERVICE_OPENED,
    VCHI_CALLBACK_BULK_RECEIVE_ABORTED,
    VCHI_CALLBACK_BULK_TRANSMIT_ABORTED,
#endif

    VCHI_CALLBACK_REASON_MAX
} VCHI_CALLBACK_REASON;

// Mailbox
#define MAILBOX_CHANNEL_VCHIQ 3

// Definition from vchiq_2835_arm.c

#define VCHIQ_TOTAL_SLOTS (VCHIQ_SLOT_ZERO_SLOTS + 2 * 32)

#define VCHIQ_MAX_FRAGMENTS (VCHIQ_NUM_CURRENT_BULKS * 2)

#define BELL0    0x00
#define BELL2    0x08

// End vchiq_2835_arm.c 

// Definition from vchiq_core.c

typedef struct _VCHIQ_OPEN_PAYLOAD {
    ULONG FourCC;
    ULONG ClientId;
    SHORT Version;
    SHORT VersionMin;
}VCHIQ_OPEN_PAYLOAD, *PVCHIQ_OPEN_PAYLOAD;

// End vchiq_core.c

EXTERN_C_END