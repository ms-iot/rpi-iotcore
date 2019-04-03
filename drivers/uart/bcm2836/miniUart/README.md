# Raspberry Pi 2 (BCM2836) Auxiliary UART (miniUART) Driver

This is a Windows 10 device driver for the Broadcom proprietary version of the 16550 UART, available on the Pi 2 and 3.

Due to lack of certain features present in the standard 16550 UART, the Broadcom version is
called miniUart.

This is a kernel mode driver implemented as a monolithic standalone serial 16550-like controller driver.
The code code is derived from the Microsoft WDF serial device driver sample fox x86/X64.

* miniUart does not support hardware flow control.
* miniUart supports software flow control (XON/XOFF).
* miniUart does not use DMA.
* Default baud rate is 9600 baud.
* Minimum baud rate is 1200 baud.
* Maximum baud rate is 912600 baud.

On Pi 3 miniUART RX/TX signals are routed to the GPIO header on pins 8/10 (GPIO15/14), 
and is available to user-mode applications (UWP or console mode) and to other device drivers. 

