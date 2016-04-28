# Raspberry Pi 2 (BCM2836) Auxiliary UART (miniUART) Driver

This is Windows 10 device driver for Broadcom proprietory version of 16550 UART. Due to lack of certain features present in standard 16550 UART
Bradcom versuion is called mini Uart, since it does not support a number of them.
The driver available on Pi3. 
The driver is a kernel mode driver implemented as a monolithyc stand alone serial 16550-like controller driver. Its code is derived from
Microsoft WDF serial device driver sample fox x86/X64.

* mini Uart serial device does not have HW flow control exposed outside
* mini Uart supports software flow control Xon/Xoff
* mini Uart does not use DMA
* default baud rate is 9600 baud
* minimum baud rate is 1200 baud
* Maximum baud rate is 912600 baud

On Pi3 mini UART RX/TX signals are routed to the GPIO header on pins 8/10 (GPIO15/14), 
and is available to user-mode applications (UWP or consle mode) and to other device drivers. 


