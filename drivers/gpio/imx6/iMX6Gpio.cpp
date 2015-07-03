//
// Freescale i.MX6 GPIO Client Driver
//
// Copyright(c) Microsoft Corporation
//
// All rights reserved.
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this software and associated documentation files(the ""Software""),
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and / or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
//
// Module Name:
//
//    iMX6Gpio.Cpp
//
// Abstract:
//
//    i.MX6 Series GPIO controller driver.
//

#include "precomp.hpp"

#include "iMX6Utility.hpp"
#include "iMX6Gpio.hpp"

IMX_NONPAGED_SEGMENT_BEGIN; //====================================================

namespace { // static

#ifdef IMX6DQ
#pragma message("Using i.MX6 Dual/Quad specific data")
    // i.MX6 Dual/Quad specific pin data
    const IMX_PIN_DATA GpioPinDataMap[] = {
    //--------------------------------------------------
    // Ctl Reg Offset|Default Value
    //--------------------------------------------------
        { 0x5F0,      0x0001B0B0 }, // GPIO1_IO00
        { 0x5F4,      0x0001B0B0 }, // GPIO1_IO01
        { 0x604,      0x0001B0B0 }, // GPIO1_IO02
        { 0x5FC,      0x0001B0B0 }, // GPIO1_IO03
        { 0x608,      0x0001B0B0 }, // GPIO1_IO04
        { 0x60C,      0x0001B0B0 }, // GPIO1_IO05
        { 0x600,      0x0001B0B0 }, // GPIO1_IO06
        { 0x610,      0x0001B0B0 }, // GPIO1_IO07
        { 0x614,      0x0001B0B0 }, // GPIO1_IO08
    };
#else
#pragma error("i.MX6 variant not supported. Please define i.MX6 variant specific data")
    const IMX_PIN_DATA GpioPinDataMap[] = {};
#endif

    static_assert(
        ARRAYSIZE(GpioPinDataMap) > 0, 
        "Should define at least 1 pin data");

} // namespace static 

_Use_decl_annotations_
NTSTATUS IMX_GPIO::ReadGpioPinsUsingMask (
    PVOID ContextPtr,
    PGPIO_READ_PINS_MASK_PARAMETERS ReadParametersPtr
    )
{
    IMX_GPIO_REGISTERS* gpioReg = static_cast<IMX_GPIO*>(ContextPtr)->gpioRegsPtr;
    IMX_GPIO_BANK_REGISTERS* bank = gpioReg->Bank + ReadParametersPtr->BankId;

    *ReadParametersPtr->PinValues = READ_REGISTER_NOFENCE_ULONG(&bank->Data);

    return STATUS_SUCCESS;
} // IMX_GPIO::ReadGpioPinsUsingMask (...)

_Use_decl_annotations_
NTSTATUS IMX_GPIO::WriteGpioPinsUsingMask (
    PVOID ContextPtr,
    PGPIO_WRITE_PINS_MASK_PARAMETERS WriteParametersPtr
    )
{
    auto thisPtr = static_cast<IMX_GPIO*>(ContextPtr);
    IMX_GPIO_REGISTERS* hw = thisPtr->gpioRegsPtr;
    IMX_GPIO_BANK_REGISTERS* bank = hw->Bank + WriteParametersPtr->BankId;
    volatile ULONG* bankDR = thisPtr->banksDataReg + WriteParametersPtr->BankId;

    (void)InterlockedOr(
        reinterpret_cast<volatile LONG*>(bankDR),
        static_cast<LONG>(WriteParametersPtr->SetMask));

    (void)InterlockedAnd(
        reinterpret_cast<volatile LONG*>(bankDR),
        static_cast<LONG>(~WriteParametersPtr->ClearMask));

    WRITE_REGISTER_NOFENCE_ULONG(&bank->Data, *bankDR);

    return STATUS_SUCCESS;
} // IMX_GPIO::WriteGpioPinsUsingMask (...)

// Although the CLIENT_Start/StopController callback function is called
// at IRQL = PASSIVE_LEVEL, you should not make this function pageable.
// The CLIENT_Start/StopController callback is in the critical timing path
// for restoring power to the devices in the hardware platform and, for 
// performance reasons, it should not be delayed by page faults
// See MSDN CLIENT_Start/StopController Remarks
_Use_decl_annotations_
NTSTATUS IMX_GPIO::StartController (
    PVOID,
    BOOLEAN /*RestoreContext*/,
    WDF_POWER_DEVICE_STATE /*PreviousPowerState*/
    )
{
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);
    return STATUS_SUCCESS;
} // IMX_GPIO::StartController (...)

_Use_decl_annotations_
NTSTATUS IMX_GPIO::StopController (
    PVOID /*ContextPtr*/,
    BOOLEAN /*SaveContext*/,
    WDF_POWER_DEVICE_STATE /*TargetState*/
    )
{
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);
    return STATUS_SUCCESS;
} // IMX_GPIO::StopController (...)

IMX_NONPAGED_SEGMENT_END; //===================================================
IMX_PAGED_SEGMENT_BEGIN; //====================================================

_Use_decl_annotations_
NTSTATUS IMX_GPIO::ConnectIoPins (
    PVOID ContextPtr,
    PGPIO_CONNECT_IO_PINS_PARAMETERS ConnectParametersPtr
    )
{
    PAGED_CODE();
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    switch (ConnectParametersPtr->ConnectMode) {
    case ConnectModeInput:
    case ConnectModeOutput:
        break;
    default:
        return STATUS_NOT_SUPPORTED;
    } // switch (ConnectParametersPtr->ConnectMode)

    IMX_GPIO_PULL pullMode;
    switch (ConnectParametersPtr->PullConfiguration) {
    case GPIO_PIN_PULL_CONFIGURATION_PULLUP:
        pullMode = IMX_GPIO_PULL_UP;
        break;
    case GPIO_PIN_PULL_CONFIGURATION_PULLDOWN:
        pullMode = IMX_GPIO_PULL_DOWN;
        break;
    case GPIO_PIN_PULL_CONFIGURATION_NONE:
        pullMode = IMX_GPIO_PULL_DISABLE;
        break;
    case GPIO_PIN_PULL_CONFIGURATION_DEFAULT:
        pullMode = IMX_GPIO_PULL_DEFAULT;
        break;
    default:
        return STATUS_NOT_SUPPORTED;
    } // switch (ConnectParametersPtr->PullConfiguration)

    auto thisPtr = static_cast<IMX_GPIO*>(ContextPtr);
    IMX_GPIO_REGISTERS* hw = thisPtr->gpioRegsPtr;
    const BANK_ID bankId = ConnectParametersPtr->BankId;
    IMX_GPIO_BANK_REGISTERS* bank = hw->Bank + bankId;
    ULONG bankDir = READ_REGISTER_NOFENCE_ULONG(&bank->Direction);
    NTSTATUS status;

    for (USHORT i = 0; i < ConnectParametersPtr->PinCount; ++i) {
        const PIN_NUMBER bankPinNumber = ConnectParametersPtr->PinNumberTable[i];
        const ULONG absolutePinNumber = IMX_MAKE_PIN(bankId, bankPinNumber);

        if (ConnectParametersPtr->ConnectMode == ConnectModeInput) {
            // when changing to an input, configure pull before changing
            // pin direction to avoid any time potentially spent floating
            status = thisPtr->updatePullMode(absolutePinNumber, pullMode);
            if (!NT_SUCCESS(status))
            {
                for (USHORT j = 0; j < i; ++j) {
                    // Revert previously modified pin pull to its default to avoid partial
                    // resource initialization
                    (void)thisPtr->updatePullMode(
                        IMX_MAKE_PIN(bankId, ConnectParametersPtr->PinNumberTable[j]),
                        IMX_GPIO_PULL_DEFAULT);
                } // for (USHORT j = ...)

                return status;
            }

            // Clear pin dir bit for input dir
            bankDir &= ~(1 << bankPinNumber);
        } else {
            // Set pin dir bit for output dir
            bankDir |= (1 << bankPinNumber);
        } // iff

    } // for (USHORT i = ...)

    WRITE_REGISTER_NOFENCE_ULONG(
        &bank->Direction,
        bankDir);

    return STATUS_SUCCESS;
} // IMX_GPIO::ConnectIoPins (...)

_Use_decl_annotations_
NTSTATUS IMX_GPIO::DisconnectIoPins (
    PVOID ContextPtr,
    PGPIO_DISCONNECT_IO_PINS_PARAMETERS DisconnectParametersPtr
    )
{
    PAGED_CODE();
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    if (DisconnectParametersPtr->DisconnectFlags.PreserveConfiguration) {
        return STATUS_SUCCESS;
    } // if

    auto thisPtr = static_cast<IMX_GPIO*>(ContextPtr);
    IMX_GPIO_REGISTERS* hw = thisPtr->gpioRegsPtr;
    const BANK_ID bankId = DisconnectParametersPtr->BankId;
    IMX_GPIO_BANK_REGISTERS* bank = hw->Bank + bankId;
    ULONG bankDir = READ_REGISTER_NOFENCE_ULONG(&bank->Direction);

    // Revert pins to input with default pull mode
    for (USHORT i = 0; i < DisconnectParametersPtr->PinCount; ++i) {
        const PIN_NUMBER bankPinNumber = DisconnectParametersPtr->PinNumberTable[i];
        const ULONG absolutePinNumber = IMX_MAKE_PIN(bankId, bankPinNumber);

        (void)thisPtr->updatePullMode(absolutePinNumber, IMX_GPIO_PULL_DEFAULT);

        // Clear pin dir bit for input dir
        bankDir &= ~(1 << bankPinNumber);
    } // for (USHORT i = ...)

    WRITE_REGISTER_NOFENCE_ULONG(
        &bank->Direction,
        bankDir);

    return STATUS_SUCCESS;
} // IMX_GPIO::DisconnectIoPins (...)

_Use_decl_annotations_
NTSTATUS IMX_GPIO::QueryControllerBasicInformation (
    PVOID /*ContextPtr*/,
    PCLIENT_CONTROLLER_BASIC_INFORMATION ControllerInformationPtr
    )
{
    PAGED_CODE();
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    ControllerInformationPtr->Version = GPIO_CONTROLLER_BASIC_INFORMATION_VERSION;
    ControllerInformationPtr->Size = static_cast<USHORT>(sizeof(*ControllerInformationPtr));
    ControllerInformationPtr->TotalPins = IMX_GPIO_PIN_COUNT;
    ControllerInformationPtr->NumberOfPinsPerBank = IMX_GPIO_PINS_PER_BANK;
    ControllerInformationPtr->Flags.MemoryMappedController = TRUE;
    ControllerInformationPtr->Flags.ActiveInterruptsAutoClearOnRead = FALSE;
    ControllerInformationPtr->Flags.FormatIoRequestsAsMasks = TRUE;
    ControllerInformationPtr->Flags.DeviceIdlePowerMgmtSupported = FALSE;
    ControllerInformationPtr->Flags.BankIdlePowerMgmtSupported = FALSE;
    ControllerInformationPtr->Flags.EmulateDebouncing = TRUE;
    ControllerInformationPtr->Flags.EmulateActiveBoth = TRUE;
    return STATUS_SUCCESS;
} // IMX_GPIO::QueryControllerBasicInformation (...)


_Use_decl_annotations_
NTSTATUS IMX_GPIO::PrepareController (
    WDFDEVICE,
    PVOID ContextPtr,
    WDFCMRESLIST /*ResourcesRaw*/,
    WDFCMRESLIST ResourcesTranslated
    )
{
    PAGED_CODE();
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    ULONG memResourceCount = 0;
    IMX_IOMUXC_REGISTERS* iomuxcRegsPtr = nullptr;
    IMX_GPIO_REGISTERS* gpioRegsPtr = nullptr;
    ULONG iomuxcRegsLength = 0;
    ULONG gpioRegsLength = 0;
    NTSTATUS status = STATUS_SUCCESS;

    // Look for single memory resource
    const ULONG resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);
    for (ULONG i = 0; i < resourceCount; ++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR res =
            WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        
        switch (res->Type) {
        case CmResourceTypeMemory:
            {
                // IOMUX Controller
                if (memResourceCount == 0) {

                    if (res->u.Memory.Length < static_cast<ULONG>(sizeof(IMX_IOMUXC_REGISTERS))) {
                        status = STATUS_DEVICE_CONFIGURATION_ERROR;
                        break;
                    } // if

                    iomuxcRegsPtr = static_cast<IMX_IOMUXC_REGISTERS*>(MmMapIoSpaceEx(
                        res->u.Memory.Start,
                        res->u.Memory.Length,
                        PAGE_READWRITE | PAGE_NOCACHE));

                    if (!iomuxcRegsPtr) {
                        status = STATUS_INSUFFICIENT_RESOURCES;
                        break;
                    } // if

                    iomuxcRegsLength = res->u.Memory.Length;
                }
                // GPIO Controller
                else if (memResourceCount == 1) {

                    if (res->u.Memory.Length < static_cast<ULONG>(sizeof(IMX_GPIO_REGISTERS))) {
                        status = STATUS_DEVICE_CONFIGURATION_ERROR;
                        break;
                    } // if

                    gpioRegsPtr = static_cast<IMX_GPIO_REGISTERS*>(MmMapIoSpaceEx(
                        res->u.Memory.Start,
                        res->u.Memory.Length,
                        PAGE_READWRITE | PAGE_NOCACHE));

                    if (!gpioRegsPtr) {
                        status = STATUS_INSUFFICIENT_RESOURCES;
                        break;
                    } // if
                    
                    gpioRegsLength = res->u.Memory.Length;

                } // iff

                ++memResourceCount;
            }
        } // switch (res->Type)
    } // for (ULONG i = ...)

    // Sanity check ACPI resources
    if (NT_SUCCESS(status) &&
        memResourceCount != 2)
    {
        status = STATUS_DEVICE_CONFIGURATION_ERROR;
    } // if

    if (NT_SUCCESS(status)) {
        // Use GPIO_CLX_RegisterClient allocated device context
        auto thisPtr = new (ContextPtr) IMX_GPIO(
            iomuxcRegsPtr,
            iomuxcRegsLength,
            gpioRegsPtr,
            gpioRegsLength);
        if (thisPtr->signature != _SIGNATURE::CONSTRUCTED) {
            status = STATUS_INTERNAL_ERROR;
        } // if
    }

    // Cleanup any claimed resources on failure
    if (!NT_SUCCESS(status))
    {
        if (iomuxcRegsPtr)
            MmUnmapIoSpace(iomuxcRegsPtr, iomuxcRegsLength);

        if (gpioRegsPtr)
            MmUnmapIoSpace(gpioRegsPtr, gpioRegsLength);
    }

    return status;
} // IMX_GPIO::PrepareController (...)

_Use_decl_annotations_
NTSTATUS IMX_GPIO::ReleaseController (
    WDFDEVICE /*WdfDevice*/,
    PVOID ContextPtr
    )
{
    PAGED_CODE();
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    auto thisPtr = static_cast<IMX_GPIO*>(ContextPtr);
    //
    // GPIOClx is memory managing the device context
    // Call the device context destructor only
    //
    if (thisPtr->signature == _SIGNATURE::CONSTRUCTED) {
        thisPtr->~IMX_GPIO();
    } // if

    return STATUS_SUCCESS;
} // IMX_GPIO::ReleaseController (...)

_Use_decl_annotations_
NTSTATUS IMX_GPIO::EvtDriverDeviceAdd (
    WDFDRIVER WdfDriver,
    WDFDEVICE_INIT* DeviceInitPtr
    )
{
    PAGED_CODE();
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    NTSTATUS status;

    WDF_OBJECT_ATTRIBUTES wdfDeviceAttributes;
    status = GPIO_CLX_ProcessAddDevicePreDeviceCreate(
        WdfDriver,
        DeviceInitPtr,
        &wdfDeviceAttributes);
    if (!NT_SUCCESS(status)) {
        return status;
    } // if

    WDFDEVICE wdfDevice;
    status = WdfDeviceCreate(
        &DeviceInitPtr,
        &wdfDeviceAttributes,
        &wdfDevice);
    switch (status) {
    case STATUS_SUCCESS: break;
    case STATUS_INSUFFICIENT_RESOURCES: return status;
    default:
        NT_ASSERT(!"Incorrect usage of WdfDeviceCreate");
        return STATUS_INTERNAL_ERROR;
    } // switch (status)

    status = GPIO_CLX_ProcessAddDevicePostDeviceCreate(WdfDriver, wdfDevice);
    if (!NT_SUCCESS(status)) {
        return status;
    } // if

    return STATUS_SUCCESS;
} // IMX_GPIO::EvtDriverDeviceAdd (...)

_Use_decl_annotations_
VOID IMX_GPIO::EvtDriverUnload (WDFDRIVER WdfDriver)
{
    PAGED_CODE();
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    GPIO_CLX_UnregisterClient(WdfDriver);
} // IMX_GPIO::EvtDriverUnload (...)

_Use_decl_annotations_
NTSTATUS IMX_GPIO::updatePullMode (
    ULONG AbsolutePinNumber,
    IMX_GPIO_PULL PullMode
    )
{
    PAGED_CODE();
    IMX_ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    if (AbsolutePinNumber >= static_cast<ULONG>(ARRAYSIZE(GpioPinDataMap)))
        return STATUS_NOT_SUPPORTED;

    IMX_IOMUXC_REGISTERS* hw = this->iomuxcRegsPtr;
    ULONG padCtlRegIndex = ::GpioPinDataMap[AbsolutePinNumber].PadCtlByteOffset / ULONG(sizeof(ULONG));

    if (PullMode == IMX_GPIO_PULL_DEFAULT) {
        ULONG padCtlDefault = ::GpioPinDataMap[AbsolutePinNumber].PadCtlDefault;
        WRITE_REGISTER_NOFENCE_ULONG(hw->Reg + padCtlRegIndex, padCtlDefault);
    } else {
        ULONG newPadCtl= READ_REGISTER_NOFENCE_ULONG(hw->Reg + padCtlRegIndex);
        newPadCtl &= ~IMX_GPIO_PULL_MASK;
        newPadCtl |= (PullMode << IMX_GPIO_PULL_SHIFT);
        WRITE_REGISTER_NOFENCE_ULONG(hw->Reg + padCtlRegIndex, newPadCtl);
    } // iff

    return STATUS_SUCCESS;
} // IMX_GPIO::updatePullMode (...)

_Use_decl_annotations_
IMX_GPIO::IMX_GPIO (
    IMX_IOMUXC_REGISTERS* IomuxcRegsPtr,
    ULONG IomuxcRegsLength,
    IMX_GPIO_REGISTERS* GpioRegsPtr,
    ULONG GpioRegsLength
    ) :
    signature(_SIGNATURE::UNINITIALIZED),
    iomuxcRegsPtr(IomuxcRegsPtr),
    iomuxcRegsLength(IomuxcRegsLength),
    gpioRegsPtr(GpioRegsPtr),
    gpioRegsLength(GpioRegsLength)
{
    PAGED_CODE();

    IMX_GPIO_BANK_REGISTERS *bank = this->gpioRegsPtr->Bank;

    for (SIZE_T i = 0; i < ARRAYSIZE(this->banksDataReg); ++i)
    {
        this->banksDataReg[i] = READ_REGISTER_NOFENCE_ULONG(&bank[i].Data);
    } // for (SIZE_T i = ...)

    this->signature = _SIGNATURE::CONSTRUCTED;
} // IMX_GPIO::IMX_GPIO (...)

_Use_decl_annotations_
IMX_GPIO::~IMX_GPIO ()
{
    PAGED_CODE();

    NT_ASSERT(this->signature == _SIGNATURE::CONSTRUCTED);
    NT_ASSERT(this->iomuxcRegsPtr);
    NT_ASSERT(this->iomuxcRegsLength);
    NT_ASSERT(this->gpioRegsPtr);
    NT_ASSERT(this->gpioRegsLength);

    MmUnmapIoSpace(this->gpioRegsPtr, this->gpioRegsLength);
    this->gpioRegsPtr = nullptr;
    this->gpioRegsLength = 0;

    MmUnmapIoSpace(this->iomuxcRegsPtr, this->iomuxcRegsLength);
    this->iomuxcRegsPtr = nullptr;
    this->iomuxcRegsLength = 0;

    this->signature = _SIGNATURE::DESTRUCTED;
} // IMX_GPIO::~IMX_GPIO ()

IMX_PAGED_SEGMENT_END; //======================================================
IMX_INIT_SEGMENT_BEGIN; //=====================================================

_Use_decl_annotations_
NTSTATUS DriverEntry (
    DRIVER_OBJECT* DriverObjectPtr,
    UNICODE_STRING* RegistryPathPtr
    )
{
    PAGED_CODE();

    NTSTATUS status;

    WDFDRIVER wdfDriver;
    {
        WDF_OBJECT_ATTRIBUTES wdfObjectAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT(&wdfObjectAttributes);

        WDF_DRIVER_CONFIG wdfDriverConfig;
        WDF_DRIVER_CONFIG_INIT(
            &wdfDriverConfig,
            IMX_GPIO::EvtDriverDeviceAdd);
        wdfDriverConfig.DriverPoolTag = IMX_GPIO_ALLOC_TAG;
        wdfDriverConfig.EvtDriverUnload = IMX_GPIO::EvtDriverUnload;

        status = WdfDriverCreate(
            DriverObjectPtr,
            RegistryPathPtr,
            &wdfObjectAttributes,
            &wdfDriverConfig,
            &wdfDriver);
        if (!NT_SUCCESS(status)) {
            return status;
        } // if
    } // wdfDriver

    // Register with GpioClx
    GPIO_CLIENT_REGISTRATION_PACKET registrationPacket = {
        GPIO_CLIENT_VERSION,
        sizeof(GPIO_CLIENT_REGISTRATION_PACKET),
        0,          // Flags
        sizeof(IMX_GPIO),
        0,          // Reserved
        IMX_GPIO::PrepareController,
        IMX_GPIO::ReleaseController,
        IMX_GPIO::StartController,
        IMX_GPIO::StopController,
        IMX_GPIO::QueryControllerBasicInformation,
        nullptr,    // CLIENT_QuerySetControllerInformation
        nullptr,    // CLIENT_EnableInterrupt
        nullptr,    // CLIENT_DisableInterrupt
        nullptr,    // CLIENT_UnmaskInterrupt
        nullptr,    // CLIENT_MaskInterrupts
        nullptr,    // CLIENT_QueryActiveInterrupts
        nullptr,    // CLIENT_ClearActiveInterrupts
        IMX_GPIO::ConnectIoPins,    // CLIENT_ConnectIoPins
        IMX_GPIO::DisconnectIoPins, // CLIENT_DisconnectIoPins
        nullptr,    // CLIENT_ReadGpioPins
        nullptr,    // CLIENT_WriteGpioPins
        nullptr,    // CLIENT_SaveBankHardwareContext
        nullptr,    // CLIENT_RestoreBankHardwareContext
        nullptr,    // CLIENT_PreProcessControllerInterrupt
        nullptr,    // CLIENT_ControllerSpecificFunction
        nullptr,    // CLIENT_ReconfigureInterrupt
        nullptr,    // CLIENT_QueryEnabledInterrupts
    }; // registrationPacket

    registrationPacket.CLIENT_ReadGpioPinsUsingMask = IMX_GPIO::ReadGpioPinsUsingMask;  // CLINET_ReadGpioPinsUsingMask

    registrationPacket.CLIENT_WriteGpioPinsUsingMask = IMX_GPIO::WriteGpioPinsUsingMask;    // CLIENT_WriteGpioPinsUsingMask

    status = GPIO_CLX_RegisterClient(
        wdfDriver,
        &registrationPacket,
        RegistryPathPtr);
    if (!NT_SUCCESS(status)) {
        return status;
    } // if

    NT_ASSERT(status == STATUS_SUCCESS);
    return status;
} // DriverEntry (...)

IMX_INIT_SEGMENT_END; //======================================================
