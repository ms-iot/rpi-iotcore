//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Module Name:
//
//    rpiq.h
//
// Abstract:
//
//    This file contains public header to interface with RPIQ driver.
//

#ifndef _RPIQ_H
#define _RPIQ_H

#include <winapifamily.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

#if (NTDDI_VERSION >= NTDDI_WINTHRESHOLD)

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_DEVICE_RPIQ        2836

//
// Symbolic Link
//
#define RPIQ_NAME L"RPIQ"
#define RPIQ_SYMBOLIC_NAME L"\\DosDevices\\" RPIQ_NAME
#define RPIQ_USERMODE_PATH L"\\\\.\\" RPIQ_NAME

#define OFFSET_DIRECT_SDRAM     0xC0000000 // Direct Access to SDRAM

//
// Interface GUID
//
// {96d104c2-6e21-49a6-8873-80d88835f763}
//
DEFINE_GUID(RPIQ_INTERFACE_GUID, 0x96d104c2, 0x6e21, 0x49a6, 0x88, 0x73, 0x80, 0xd8, 0x88, 0x35, 0xf7, 0x63 );

//
// RPIQ IOCTL definition
//
enum RPIQFunction
{
    RPIQ_FUNC_MIN = 2000,
    RPIQ_FUNC_MAILBOX_POWER_MANAGEMENT = 2000,
    RPIQ_FUNC_MAILBOX_FRAME_BUFFER = 2001,
    RPIQ_FUNC_MAILBOX_VIRT_UART = 2002,
    RPIQ_FUNC_MAILBOX_VCHIQ = 2003,
    RPIQ_FUNC_MAILBOX_LED = 2004,
    RPIQ_FUNC_MAILBOX_BUTTONS = 2005,
    RPIQ_FUNC_MAILBOX_TOUCH_SCREEN = 2006,
    RPIQ_FUNC_MAILBOX_UNKNOWN = 2007,
    RPIQ_FUNC_MAILBOX_PROPERTY = 2008,
    RPIQ_FUNC_MAILBOX_PROPERTY_VC = 2009,

    RPIQ_FUNC_MAX = 4000
};

#define IOCTL_MAILBOX_POWER_MANAGEMENT \
    CTL_CODE(FILE_DEVICE_RPIQ, RPIQ_FUNC_MAILBOX_POWER_MANAGEMENT, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MAILBOX_FRAME_BUFFER \
    CTL_CODE(FILE_DEVICE_RPIQ, RPIQ_FUNC_MAILBOX_FRAME_BUFFER, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MAILBOX_VIRT_UART \
    CTL_CODE(FILE_DEVICE_RPIQ, RPIQ_FUNC_MAILBOX_VIRT_UART, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MAILBOX_VCHIQ \
    CTL_CODE(FILE_DEVICE_RPIQ, RPIQ_FUNC_MAILBOX_VCHIQ, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MAILBOX_LED \
    CTL_CODE(FILE_DEVICE_RPIQ, RPIQ_FUNC_MAILBOX_LED, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MAILBOX_BUTTONS \
    CTL_CODE(FILE_DEVICE_RPIQ, RPIQ_FUNC_MAILBOX_BUTTONS, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MAILBOX_TOUCH_SCREEN \
    CTL_CODE(FILE_DEVICE_RPIQ, RPIQ_FUNC_MAILBOX_TOUCH_SCREEN, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MAILBOX_PROPERTY \
    CTL_CODE(FILE_DEVICE_RPIQ, RPIQ_FUNC_MAILBOX_PROPERTY, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// Tag Request
//
#define TAG_REQUEST         0x00000000

//
// Response
//
#define RESPONSE_SUCCESS    0x80000000
#define RESPONSE_ERROR      0x80000001

#include <pshpack1.h>

//
// Mailbox Property Interface
//

// Standard mailbox header for all property interface
typedef struct _MAILBOX_HEADER {
    ULONG TotalBuffer;
    ULONG RequestResponse;
    ULONG TagID;
    ULONG ResponseLength;
    ULONG Request;
} MAILBOX_HEADER, *PMAILBOX_HEADER;

//
// Get firmware revision
//
// Tag : 0x00000001
//   Request :
//    Length : 0
//   Response :
//    Length : 4
//    Value :
//    u32 : firmware revision
//
#define TAG_ID_GET_FIRMWARE_REVISION  0x00000001

typedef struct _MAILBOX_GET_FIRMWARE_REVISION {
    MAILBOX_HEADER Header;
    ULONG FirmwareRevision;
    ULONG EndTag;
} MAILBOX_GET_FIRMWARE_REVISION, *PMAILBOX_GET_FIRMWARE_REVISION;

__inline VOID INIT_MAILBOX_GET_FIRMWARE_REVISION (
    _Out_ MAILBOX_GET_FIRMWARE_REVISION* PropertyMsgPtr
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_FIRMWARE_REVISION);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_FIRMWARE_REVISION;
    PropertyMsgPtr->Header.ResponseLength = 4;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->FirmwareRevision = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get board revision
//
// Tag : 0x00010001
//   Request :
//    Length : 0
//   Response :
//    Length : 4
//    Value :
//    u32 : Board model
//
#define TAG_ID_GET_BOARD_MODEL  0x00010001

typedef struct _MAILBOX_GET_BOARD_MODEL
{
    MAILBOX_HEADER Header;
    ULONG BoardModel;
    ULONG EndTag;
} MAILBOX_GET_BOARD_MODEL, *PMAILBOX_GET_BOARD_MODEL;

__inline VOID INIT_MAILBOX_GET_BOARD_MODEL (
    _Out_ MAILBOX_GET_BOARD_MODEL* PropertyMsgPtr
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_BOARD_MODEL);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_BOARD_MODEL;
    PropertyMsgPtr->Header.ResponseLength = 4;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->BoardModel = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get board revision
//
// Tag : 0x00010002
//   Request :
//    Length : 0
//   Response :
//    Length : 4
//    Value :
//    u32 : Board revision
//
#define TAG_ID_GET_BOARD_REVISION  0x00010002

typedef struct _MAILBOX_GET_BOARD_REVISION {
    MAILBOX_HEADER Header;
    ULONG BoardRevision;
    ULONG EndTag;
} MAILBOX_GET_BOARD_REVISION, *PMAILBOX_GET_BOARD_REVISION;

__inline VOID INIT_MAILBOX_GET_BOARD_REVISION (
    _Out_ MAILBOX_GET_BOARD_REVISION* PropertyMsgPtr
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_BOARD_REVISION);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_BOARD_REVISION;
    PropertyMsgPtr->Header.ResponseLength = 4;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->BoardRevision = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get board MAC address
//
// Tag : 0x00010003
//   Request :
//    Length : 0
//   Response :
//    Length : 6
//    Value :
//    u8 : MAC Address
//
#define TAG_ID_GET_BOARD_MAC_ADDRESS  0x00010003

typedef struct _MAILBOX_GET_MAC_ADDRESS {
    MAILBOX_HEADER Header;
    BYTE MACAddress[6];
    BYTE Padding[2];
    ULONG EndTag;
} MAILBOX_GET_MAC_ADDRESS, *PMAILBOX_GET_MAC_ADDRESS;

__inline VOID INIT_MAILBOX_GET_BOARD_MAC_ADDRESS (
    _Out_ MAILBOX_GET_MAC_ADDRESS* PropertyMsgPtr
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_MAC_ADDRESS);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_BOARD_MAC_ADDRESS;
    PropertyMsgPtr->Header.ResponseLength = 6;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->Padding[0] = 0;
    PropertyMsgPtr->Padding[1] = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get board board serial
//
// Tag : 0x00010004
//   Request :
//    Length : 0
//   Response :
//    Length : 8
//    Value :
//    u64 : Board serial
//
#define TAG_ID_GET_BOARD_SERIAL  0x00010004

typedef struct _MAILBOX_GET_BOARD_SERIAL {
    MAILBOX_HEADER Header;
    BYTE BoardSerial[8];
    ULONG EndTag;
} MAILBOX_GET_BOARD_SERIAL, *PMAILBOX_GET_BOARD_SERIAL;

__inline VOID INIT_MAILBOX_GET_BOARD_SERIAL (
    _Out_ MAILBOX_GET_BOARD_SERIAL* PropertyMsgPtr
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_BOARD_SERIAL);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_BOARD_SERIAL;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get ARM memory
//
// Tag : 0x00010005
//   Request :
//    Length : 0
//   Response :
//    Length : 8
//    Value :
//    u32 : Base address in bytes
//    u32 : Size in bytes
//
#define TAG_ID_GET_ARM_MEMORY  0x00010005

typedef struct _MAILBOX_GET_ARM_MEMORY {
    MAILBOX_HEADER Header;
    ULONG BaseAddress;
    ULONG Size;
    ULONG EndTag;
} MAILBOX_GET_ARM_MEMORY, *PMAILBOX_GET_ARM_MEMORY;

__inline VOID INIT_MAILBOX_GET_ARM_MEMORY (
    _Out_ MAILBOX_GET_ARM_MEMORY* PropertyMsgPtr
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_ARM_MEMORY);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_ARM_MEMORY;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->BaseAddress = 0;
    PropertyMsgPtr->Size = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get VC memory
//
// Tag : 0x00010006
//   Request :
//    Length : 0
//   Response :
//    Length : 8
//    Value :
//    u32 : Base address in bytes
//    u32 : Size in bytes
//
#define TAG_ID_GET_VC_MEMORY  0x00010006

typedef struct _MAILBOX_GET_VC_MEMORY {
    MAILBOX_HEADER Header;
    ULONG BaseAddress;
    ULONG Size;
    ULONG EndTag;
} MAILBOX_GET_VC_MEMORY, *PMAILBOX_GET_VC_MEMORY;

__inline VOID INIT_MAILBOX_GET_VC_MEMORY (
    _Out_ MAILBOX_GET_VC_MEMORY* PropertyMsgPtr
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_VC_MEMORY);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_VC_MEMORY;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->BaseAddress = 0;
    PropertyMsgPtr->Size = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Mailbox Clock Id
//

#define MAILBOX_CLOCK_ID_RESERVED   0x00000000
#define MAILBOX_CLOCK_ID_EMMC       0x00000001
#define MAILBOX_CLOCK_ID_UART       0x00000002
#define MAILBOX_CLOCK_ID_ARM        0x00000003
#define MAILBOX_CLOCK_ID_CORE       0x00000004
#define MAILBOX_CLOCK_ID_V3D        0x00000005
#define MAILBOX_CLOCK_ID_H264       0x00000006
#define MAILBOX_CLOCK_ID_ISP        0x00000007
#define MAILBOX_CLOCK_ID_SDRAM      0x00000008
#define MAILBOX_CLOCK_ID_PIXEL      0x00000009
#define MAILBOX_CLOCK_ID_PWM        0x0000000A

//
// Get Clock Rate
//
// Tag : 0x00030002
//   Request :
//    Length : 4
//    Value :
//    u32 : clock id
//   Response :
//    Length : 8
//    Value :
//    u32 : clock id
//    u32 : rate (in Hz)
//
#define TAG_ID_GET_CLOCK_RATE  0x00030002

typedef struct _MAILBOX_GET_CLOCK_RATE {
    MAILBOX_HEADER Header;
    ULONG ClockId;
    ULONG Rate;
    ULONG EndTag;
} MAILBOX_GET_CLOCK_RATE, *PMAILBOX_GET_CLOCK_RATE;

__inline VOID INIT_MAILBOX_GET_CLOCK_RATE (
    _Out_ MAILBOX_GET_CLOCK_RATE* PropertyMsgPtr,
    _In_ ULONG ClockId
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_CLOCK_RATE);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_CLOCK_RATE;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->ClockId = ClockId;
    PropertyMsgPtr->Rate = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Set Clock Rate
//
// Tag : 0x00038002
//   Request :
//    Length : 12
//    Value :
//    u32 : clock id
//    u32 : rate (in Hz)
//    u32 : skip setting turbo
//   Response :
//    Length : 8
//    Value :
//    u32 : clock id
//    u32 : rate (in Hz)
//
#define TAG_ID_SET_CLOCK_RATE  0x00038002

typedef struct _MAILBOX_SET_CLOCK_RATE {
    MAILBOX_HEADER Header;
    ULONG ClockId;
    ULONG Rate;
    ULONG SkipSettingTurbo;
    ULONG EndTag;
} MAILBOX_SET_CLOCK_RATE, *PMAILBOX_SET_CLOCK_RATE;

__inline VOID INIT_MAILBOX_SET_CLOCK_RATE (
    _Out_ MAILBOX_SET_CLOCK_RATE* PropertyMsgPtr,
    _In_ ULONG ClockId,
    _In_ ULONG Rate,
    _In_ ULONG SkipSettingTurbo
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_SET_CLOCK_RATE);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_SET_CLOCK_RATE;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->ClockId = ClockId;
    PropertyMsgPtr->Rate = Rate;
    PropertyMsgPtr->SkipSettingTurbo = SkipSettingTurbo;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get Max Clock Rate
//
// Tag : 0x00030004
//   Request :
//    Length : 4
//    Value :
//    u32 : clock id
//   Response :
//    Length : 8
//    Value :
//    u32 : clock id
//    u32 : rate (in Hz)
//
#define TAG_ID_GET_MAX_CLOCK_RATE  0x00030004

__inline VOID INIT_MAILBOX_GET_MAX_CLOCK_RATE (
    _Out_ MAILBOX_GET_CLOCK_RATE* PropertyMsgPtr,
    _In_ ULONG ClockId
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_CLOCK_RATE);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_MAX_CLOCK_RATE;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->ClockId = ClockId;
    PropertyMsgPtr->Rate = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get Min Clock Rate
//
// Tag : 0x00030007
//   Request :
//    Length : 4
//    Value :
//    u32 : clock id
//   Response :
//    Length : 8
//    Value :
//    u32 : clock id
//    u32 : rate (in Hz)
//
#define TAG_ID_GET_MIN_CLOCK_RATE  0x00030007

__inline VOID INIT_MAILBOX_GET_MIN_CLOCK_RATE (
    _Out_ MAILBOX_GET_CLOCK_RATE* PropertyMsgPtr,
    _In_ ULONG ClockId
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_CLOCK_RATE);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_MIN_CLOCK_RATE;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->ClockId = ClockId;
    PropertyMsgPtr->Rate = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Set V3D power state
//
// Tag : 0x00030012
//   Request :
//    Length : 4
//    Value :
//    u32 : V3D power state
//   Response :
//    Length : 4
//    Value :
//    u32 : V3D power state
//
#define TAG_ID_SET_POWER_VC4  0x00030012

typedef struct _MAILBOX_SET_POWER_VC4 {
    MAILBOX_HEADER Header;
    ULONG PowerOn;
    ULONG EndTag;
} MAILBOX_SET_POWER_VC4, *PMAILBOX_SET_POWER_VC4;

__inline VOID INIT_MAILBOX_SET_POWER_VC4 (
    _Out_ MAILBOX_SET_POWER_VC4* PropertyMsgPtr,
    _In_ ULONG PowerOn
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_SET_POWER_VC4);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_SET_POWER_VC4;
    PropertyMsgPtr->Header.ResponseLength = 4;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->PowerOn = PowerOn;
    PropertyMsgPtr->EndTag = 0;
}

//
// Allocate Memory
//
// Tag : 0x0003000c
//   Request :
//    Length : 12
//    Value :
//    u32 : size
//    u32 : alignment
//    u32 : flags
//   Response :
//    Length : 4
//    Value :
//    u32 : handle
//
#define TAG_ID_ALLOC_MEM  0x0003000c
#define ALIGN_4K          4*1024
typedef struct _MAILBOX_ALLOC_MEM {
    MAILBOX_HEADER Header;
    ULONG Size;
    ULONG Aligment;
    ULONG Flag;
    ULONG EndTag;
} MAILBOX_ALLOC_MEM, *PMAILBOX_ALLOC_MEM;

__inline VOID INIT_MAILBOX_ALLOC_MEM (
    _Out_ MAILBOX_ALLOC_MEM* PropertyMsgPtr,
    _In_ ULONG Size,
    _In_ ULONG Aligment
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_ALLOC_MEM);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_ALLOC_MEM;
    PropertyMsgPtr->Header.ResponseLength = 12;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->Size = Size;
    PropertyMsgPtr->Aligment = Aligment;
    PropertyMsgPtr->Flag = 0x0000000C;
    PropertyMsgPtr->EndTag = 0;
}

//
// Lock memory
//
// Tag : 0x0003000d
//   Request :
//    Length : 4
//    Value :
//    u32 : handle
//   Response :
//    Length : 4
//    Value :
//    u32 : bus Address
//
#define TAG_ID_LOCK_MEM  0x0003000d
typedef struct _MAILBOX_LOCK_MEM {
    MAILBOX_HEADER Header;
    ULONG Handle;
    ULONG EndTag;
} MAILBOX_LOCK_MEM, *PMAILBOX_LOCK_MEM;

__inline VOID INIT_MAILBOX_LOC_MEM (
    _Out_ MAILBOX_LOCK_MEM* PropertyMsgPtr,
    _In_ ULONG Handle
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_LOCK_MEM);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_LOCK_MEM;
    PropertyMsgPtr->Header.ResponseLength = 4;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->Handle = Handle;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get EDID block
//
// Tag : 0x00030020
//   Request :
//    Length : 4
//    Value :
//    u32 : block number
//   Response :
//    Length : 136
//    Value :
//    u32 : block number
//    u32 : status  (keep requesting blocks until this is nonzero)
//    128 bytes: EDID block
//
#define TAG_ID_GET_EDID 0x00030020
typedef struct _MAILBOX_GET_EDID {
    MAILBOX_HEADER Header;    
    ULONG BlockNumber;
    ULONG Status;
    BYTE Edid[128];
    ULONG EndTag;
} MAILBOX_GET_EDID, *PMAILBOX_GET_EDID;

__inline VOID INIT_MAILBOX_GET_EDID (
    _Out_ MAILBOX_GET_EDID* PropertyMsgPtr,
    _In_ ULONG BlockNumber
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_EDID);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_EDID;
    PropertyMsgPtr->Header.ResponseLength = 136;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->BlockNumber = BlockNumber;
    PropertyMsgPtr->EndTag = 0;
}

//
// Get virtual (buffer) width/height
//
// Tag : 0x00040004
//   Request :
//    Length : 0
//   Response :
//    Length : 8
//    Value :
//    u32 : width in pixels
//    u32 : height in pixels
//
#define TAG_ID_GET_VIRTUAL_BUFFER_SIZE 0x00040004
typedef struct _MAILBOX_GET_VIRTUAL_BUFFER_SIZE {
    MAILBOX_HEADER Header;    
    ULONG WidthPixels;
    ULONG HeightPixels;
    ULONG EndTag;
} MAILBOX_GET_VIRTUAL_BUFFER_SIZE, *PMAILBOX_GET_VIRTUAL_BUFFER_SIZE;

__inline VOID INIT_MAILBOX_GET_VIRTUAL_BUFFER_SIZE (
    _Out_ MAILBOX_GET_VIRTUAL_BUFFER_SIZE* PropertyMsgPtr
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_VIRTUAL_BUFFER_SIZE);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_VIRTUAL_BUFFER_SIZE;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->EndTag = 0;
}

//
// Set Cursor Info
//
// Tag: 0x00008010 Error in wiki documentation. Correct value define here
// https://github.com/raspberrypi/linux/blob/e50d6adf1df06a1d4f8e5938c23ed7c3502ed02d/arch/arm/mach-bcm2708/include/mach/vcio.h
//   Request :
//    Length : 24
//    Value :
//    u32 : Width
//    u32 : Height
//    u32 : (unused)
//    u32 : pointer to pixels
//    u32 : hotspotX
//    u32 : hotspotY
//   Response :
//    Length : 4
//    Value :
//    u32 : 0 = valid, 1 = invalid
//
#define TAG_ID_SET_CURSOR_INFO  0x00008010
#define MAX_CURSOR_WIDTH    64
#define MAX_CURSOR_HEIGHT   64
#define CURSOR_BPP          4
#define MAX_CURSOR_MEMORY   MAX_CURSOR_WIDTH * MAX_CURSOR_HEIGHT * PropertyMsgPtrURSOR_BPP

typedef struct _MAILBOX_SET_CURSOR_INFO {
    MAILBOX_HEADER Header;
    ULONG Width;
    ULONG Height;
    ULONG Unused;
    ULONG PointerToPixel;
    ULONG HotspotX;
    ULONG HotspotY;
    ULONG EndTag;
} MAILBOX_SET_CURSOR_INFO;

__inline VOID INIT_MAILBOX_CURSOR_INFO (
    _Out_ MAILBOX_SET_CURSOR_INFO* PropertyMsgPtr,
    _In_ ULONG Width,
    _In_ ULONG Height,
    _In_ ULONG Address
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_SET_CURSOR_INFO);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_SET_CURSOR_INFO;
    PropertyMsgPtr->Header.ResponseLength = 24;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->Width = Width;
    PropertyMsgPtr->Height = Height;
    PropertyMsgPtr->Unused = 0;
    PropertyMsgPtr->PointerToPixel = Address;
    PropertyMsgPtr->HotspotX = 0;
    PropertyMsgPtr->HotspotY = 0;
    PropertyMsgPtr->EndTag = 0;
}

//
// Set Cursor State
//
// Tag : 0x00008011 Error in wiki documentation. Correct value define here
// https://github.com/raspberrypi/linux/blob/e50d6adf1df06a1d4f8e5938c23ed7c3502ed02d/arch/arm/mach-bcm2708/include/mach/vcio.h
//   Request :
//    Length : 16
//    Value :
//    u32 : enable(1 = visible, 0 = invisible)
//    u32 : x
//    u32 : y
//    u32 : flags
//   Response :
//    Length : 4
//    Value :
//    u32 : 0 = valid, 1 = invalid
//
#define TAG_ID_SET_CURSOR_STATE  0x00008011
typedef struct _MAILBOX_SET_CURSOR_STATE {
    MAILBOX_HEADER Header;
    ULONG Enable;
    ULONG HotspotX;
    ULONG HotspotY;
    ULONG Flags;
    ULONG EndTag;
} MAILBOX_SET_CURSOR_STATE, *PMAILBOX_SET_CURSOR_STATE;

__inline VOID INIT_MAILBOX_CURSOR_STATE (
    _Out_ MAILBOX_SET_CURSOR_STATE* PropertyMsgPtr,
    _In_ ULONG Enable,
    _In_ ULONG HotspotX,
    _In_ ULONG HotspotY
    )
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_SET_CURSOR_STATE);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_SET_CURSOR_STATE;
    PropertyMsgPtr->Header.ResponseLength = 16;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->Enable = Enable;
    PropertyMsgPtr->HotspotX = HotspotX;
    PropertyMsgPtr->HotspotY = HotspotY;
    PropertyMsgPtr->EndTag = 0;
}

//
// GPIO Expander
// Tag: 0x00030041 and 0x00038041
//   Request :
//    Length : 8
//    Value :
//    u32 : Gpio id
//    u32 : Gpio Set State
//   Response :
//    Length : 8
//    Value :
//    u32 : Gpio id
//    u32 : Gpio State
//
#define TAG_ID_GET_GPIO_EXPANDER  0x00030041
#define TAG_ID_SET_GPIO_EXPANDER  0x00038041
typedef struct _MAILBOX_GET_SET_GPIO_EXPANDER
{
    MAILBOX_HEADER Header;
    ULONG GpioId;
    ULONG GpioState;
    ULONG EndTag;
} MAILBOX_GET_SET_GPIO_EXPANDER, *PMAILBOX_GET_SET_GPIO_EXPANDER;

__inline VOID INIT_MAILBOX_GET_GPIO_EXPANDER (
    _Out_ MAILBOX_GET_SET_GPIO_EXPANDER* PropertyMsgPtr,
    _In_ ULONG GpioId)
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_SET_GPIO_EXPANDER);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_GPIO_EXPANDER;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->GpioId = GpioId;
    PropertyMsgPtr->EndTag = 0;
}

__inline VOID INIT_MAILBOX_SET_GPIO_EXPANDER (
    _Out_ MAILBOX_GET_SET_GPIO_EXPANDER* PropertyMsgPtr,
    _In_ ULONG GpioId,
    _In_ ULONG GpioState)
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_SET_GPIO_EXPANDER);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_SET_GPIO_EXPANDER;
    PropertyMsgPtr->Header.ResponseLength = 8;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->GpioId = GpioId;
    PropertyMsgPtr->GpioState = GpioState;
    PropertyMsgPtr->EndTag = 0;
}

// Get touch buffer
// Tag: 0x0004000f
//   Request :
//    Length : 4 bytes
//    Value :  null
//   Response :
//    Length : 4 bytes
//    Value :  UINT32
//    u32 : Touch buffer address if any, null otherwise
//
#define TAG_ID_GET_TOUCHBUF  0x0004000f // RPI_FIRMWARE_FRAMEBUFFER_GET_TOUCHBUF=0x0004000f
typedef struct _MAILBOX_GET_TOUCH_BUF
{
    MAILBOX_HEADER Header;
    ULONG TouchBuffer;
    ULONG EndTag;
} MAILBOX_GET_TOUCH_BUF;

__inline VOID INIT_MAILBOX_GET_TOUCH_BUFF (
    _Out_ MAILBOX_GET_TOUCH_BUF* PropertyMsgPtr,
    _In_ ULONG InTouchBuffer)
{
    PropertyMsgPtr->Header.TotalBuffer = sizeof(MAILBOX_GET_TOUCH_BUF);
    PropertyMsgPtr->Header.RequestResponse = TAG_REQUEST;
    PropertyMsgPtr->Header.TagID = TAG_ID_GET_TOUCHBUF;
    PropertyMsgPtr->Header.ResponseLength = 4;
    PropertyMsgPtr->Header.Request = TAG_REQUEST;
    PropertyMsgPtr->TouchBuffer = InTouchBuffer;
    PropertyMsgPtr->EndTag=0;
}

#include <poppack.h>

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // NTDDI_VERSION >= NTDDI_WINTHRESHOLD

#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

#endif //_RPIQ_H