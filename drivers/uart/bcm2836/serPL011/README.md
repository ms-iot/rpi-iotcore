# Raspberry Pi 2 (BCM2836) ARM PL011 UART Driver
This is the Arm standard PL011 UART driver available on Pi2 and Pi3.
The driver is a kernel mode driver implemented as a SerCx2 Serial Controller Driver.
The device does not have HW flow control and does not use DMA.

On Pi2 the PL011 UART RX/TX signals are routed to the Pi2 header on pins 8/10 (GPIO15/14), and is available to user-mode application and other device drivers.
On Pi3 it is being used by the BT stack to communicate with the BT modem, and thus not available to user-mode application and other device drivers.
On Pi3 the miniUART is used for this purpose.

The PL011 UART registry settings reside under key HKLM\System\CurrentControlSet\services\SerPl011\Parameters:
- UartClockHz: UART clock [Hz]. Default value is 16 Mhz. The UART clock needs to be 16 times the maximum baud rate, means a default of 1 MBPS.
- MaxBaudRateBPS: Maximum baud rate [Bytes Per Second], default is 921600 BPS.
