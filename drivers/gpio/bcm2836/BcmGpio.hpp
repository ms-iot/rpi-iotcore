#ifndef _BCMGPIO_HPP_
#define _BCMGPIO_HPP_ 1
//
// Copyright (C) Microsoft.  All rights reserved.
//
//
// Module Name:
//
//  BcmGpio.hpp
//
// Abstract:
//
//    This file contains types and definitions for BCM2836 GPIO
//    controller driver.
//
// Environment:
//
//    Kernel mode only.
//

enum : ULONG {
    BCM_GPIO_ALLOC_TAG = 'GMCB'
};

enum {
    BCM_GPIO_PIN_COUNT = 54,
    BCM_GPIO_PINS_PER_BANK = 32,
    BCM_GPIO_BANK_COUNT = 2
};

enum BCM_GPIO_PULL {
    BCM_GPIO_PULL_DISABLE = 0,
    BCM_GPIO_PULL_DOWN,
    BCM_GPIO_PULL_UP
};

enum BCM_GPIO_FUNCTION {
    BCM_GPIO_FUNCTION_INPUT = 0x0,
    BCM_GPIO_FUNCTION_OUTPUT = 0x1,
    BCM_GPIO_FUNCTION_ALT0 = 0x4,
    BCM_GPIO_FUNCTION_ALT1 = 0x5,
    BCM_GPIO_FUNCTION_ALT2 = 0x6,
    BCM_GPIO_FUNCTION_ALT3 = 0x7,
    BCM_GPIO_FUNCTION_ALT4 = 0x3,
    BCM_GPIO_FUNCTION_ALT5 = 0x2,
};

#include <pshpack4.h>  //====================================================

struct BCM_GPIO_REGISTERS {
    ULONG GPFSEL[6];
    ULONG Reserved1;
    ULONG GPSET[BCM_GPIO_BANK_COUNT];
    ULONG Reserved2;
    ULONG GPCLR[BCM_GPIO_BANK_COUNT];
    ULONG Reserved3;
    ULONG GPLEV[BCM_GPIO_BANK_COUNT];
    ULONG Reserved4;
    ULONG GPEDS[BCM_GPIO_BANK_COUNT];
    ULONG Reserved5;
    ULONG GPREN[BCM_GPIO_BANK_COUNT];
    ULONG Reserved6;
    ULONG GPFEN[BCM_GPIO_BANK_COUNT];
    ULONG Reserved7;
    ULONG GPHEN[BCM_GPIO_BANK_COUNT];
    ULONG Reserved8;
    ULONG GPLEN[BCM_GPIO_BANK_COUNT];
    ULONG Reserved9;
    ULONG GPAREN[BCM_GPIO_BANK_COUNT];
    ULONG Reserved10;
    ULONG GPAFEN[BCM_GPIO_BANK_COUNT];
    ULONG Reserved11;
    ULONG GPPUD;
    ULONG GPPUDCLK[BCM_GPIO_BANK_COUNT];
    ULONG Reserved12[4];
    ULONG Test;
}; // struct BCM_GPIO_REGISTERS

#include <poppack.h> //======================================================

class BCM_GPIO {
public: // NONPAGED

    // Number of times a pin can cause the ISR to run without allowing the
    // DPC to run before interrupts on that pin are temporarily disabled.
    // This value is determined from experimentation.
    enum : UCHAR {
        WATCHDOG_RESET = 10
    };

    // Shadows the hardware interrupt configuration registers. These values
    // are shadowed because read/modify/write sequences were observed to be
    // unreliable in testing.
    struct _INTERRUPT_REGISTERS {

        _INTERRUPT_REGISTERS () : GPHEN(), GPLEN(), GPAREN(), GPAFEN() { }

        ULONG EnabledMask () const
        {
            return this->GPHEN | this->GPLEN | this->GPAREN | this->GPAFEN;
        } // EnabledMask ()

        void Add (
            ULONG Mask,
            KINTERRUPT_MODE InterruptMode,
            KINTERRUPT_POLARITY Polarity
            );

        void Remove ( ULONG Mask )
        {
            this->GPHEN &= ~Mask;
            this->GPLEN &= ~Mask;
            this->GPAREN &= ~Mask;
            this->GPAFEN &= ~Mask;
        } // Remove (...)

        ULONG GPHEN;
        ULONG GPLEN;
        ULONG GPAREN;
        ULONG GPAFEN;
    }; // struct _INTERRUPT_REGISTERS

    class _INTERRUPT_CONTEXT {
        friend class BCM_GPIO;

        _INTERRUPT_CONTEXT () : bankId(ULONG(-1)) { }

        void initialize (ULONG BankId, WDFDPC WdfDpc, WDFTIMER WdfTimer)
        {
            this->bankId = BankId;
            this->dpc = WdfDpc;
            this->interruptReenableTimer = WdfTimer;
            this->resetWatchdogCount();
        } // initialize (...)

        void resetWatchdogCount ()
        {
            RtlFillMemory(
                this->watchdogCount,
                sizeof(this->watchdogCount),
                WATCHDOG_RESET);
        } // resetWatchdogCount ()

        ULONG bankId;
        ULONG enabledMask;
        ULONG disabledMask;
        ULONG pendingReenableMask;
        _INTERRUPT_REGISTERS registers;
        WDFDPC dpc;
        WDFTIMER interruptReenableTimer;
        UCHAR watchdogCount[BCM_GPIO_PINS_PER_BANK];
    }; // class _INTERRUPT_CONTEXT

    class _DPC_CONTEXT {
        friend class BCM_GPIO;

        _INTERRUPT_CONTEXT* interruptContextPtr;
        BCM_GPIO* thisPtr;
    }; // class _DPC_CONTEXT

    static GPIO_CLIENT_PRE_PROCESS_CONTROLLER_INTERRUPT
        PreProcessControllerInterrupt;
    static GPIO_CLIENT_MASK_INTERRUPTS MaskInterrupts;
    static GPIO_CLIENT_UNMASK_INTERRUPT UnmaskInterrupt;
    static GPIO_CLIENT_QUERY_ACTIVE_INTERRUPTS QueryActiveInterrupts;
    static GPIO_CLIENT_CLEAR_ACTIVE_INTERRUPTS ClearActiveInterrupts;
    static GPIO_CLIENT_QUERY_ENABLED_INTERRUPTS QueryEnabledInterrupts;
    static GPIO_CLIENT_RECONFIGURE_INTERRUPT ReconfigureInterrupt;

    static GPIO_CLIENT_READ_PINS_MASK ReadGpioPinsUsingMask;
    static GPIO_CLIENT_WRITE_PINS_MASK WriteGpioPinsUsingMask;

    static GPIO_CLIENT_START_CONTROLLER StartController;
    static GPIO_CLIENT_STOP_CONTROLLER StopController;

    static GPIO_CLIENT_ENABLE_INTERRUPT EnableInterrupt;
    static GPIO_CLIENT_DISABLE_INTERRUPT DisableInterrupt;

    static GPIO_CLIENT_CONNECT_FUNCTION_CONFIG_PINS ConnectFunctionConfigPins;
    static GPIO_CLIENT_DISCONNECT_FUNCTION_CONFIG_PINS DisconnectFunctionConfigPins;

private: // NONPAGED

    static EVT_WDF_DPC evtDpcFunc;

    void programInterruptRegisters ( ULONG BankId );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void setDriveMode (
        BANK_ID BankId,
        PIN_NUMBER PinNumber,
        BCM_GPIO_FUNCTION Function,
        UCHAR AcpiPullConfig
        );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void revertPinToDefault (BANK_ID BankId, PIN_NUMBER PinNumber);

    BCM_GPIO_REGISTERS* registersPtr;
    _INTERRUPT_CONTEXT interruptContext[BCM_GPIO_BANK_COUNT];
    BITFIELD_ARRAY<BCM_GPIO_PIN_COUNT, 3> gpfsel;
    BITFIELD_ARRAY<BCM_GPIO_PIN_COUNT, 3> initialGpfsel;
    BITFIELD_ARRAY<BCM_GPIO_PIN_COUNT, 2> pullConfig;
    BITFIELD_ARRAY<BCM_GPIO_PIN_COUNT, 2> defaultPullConfig;
    ULONG openIoPins[BCM_GPIO_BANK_COUNT];
    ULONG openInterruptPins[BCM_GPIO_BANK_COUNT];
    
    ULONG registersLength;
    enum class _SIGNATURE {
        UNINITIALIZED = 0,
        CONSTRUCTED = 'GMCB',
        DESTRUCTED = 'gmcb',
    } signature;

public: // PAGED

    static GPIO_CLIENT_CONNECT_IO_PINS ConnectIoPins;
    static GPIO_CLIENT_DISCONNECT_IO_PINS DisconnectIoPins;

    static GPIO_CLIENT_QUERY_CONTROLLER_BASIC_INFORMATION
        QueryControllerBasicInformation;

    static GPIO_CLIENT_PREPARE_CONTROLLER PrepareController;
    static GPIO_CLIENT_RELEASE_CONTROLLER ReleaseController;

    static EVT_WDF_DRIVER_DEVICE_ADD EvtDriverDeviceAdd;
    static EVT_WDF_DRIVER_UNLOAD EvtDriverUnload;

private: // PAGED

    static EVT_WDF_TIMER evtReenableInterruptTimerFunc;

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void updatePullMode (
        BANK_ID BankId,
        PIN_NUMBER PinNumber,
        BCM_GPIO_PULL PullMode
        );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    NTSTATUS initialize ( WDFDEVICE WdfDevice );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    BCM_GPIO (
        _In_ BCM_GPIO_REGISTERS* registersPtr,
        ULONG registersLength
        );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    ~BCM_GPIO ();

    BCM_GPIO (const BCM_GPIO&) = delete;
    BCM_GPIO& operator= (const BCM_GPIO&) = delete;
}; // class BCM_GPIO

extern "C" DRIVER_INITIALIZE DriverEntry;

#endif // _BCMGPIO_HPP_
