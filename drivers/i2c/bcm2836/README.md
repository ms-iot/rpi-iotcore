# Raspberry Pi 2 (BCM2836) I2C Controller driver for SPB Framework

This is the I2C driver for Raspberry Pi. It is implemented as an 
[SpbCx Controller Driver](https://msdn.microsoft.com/en-us/library/windows/hardware/hh406203(v=vs.85).aspx).
I2C1 is exposed to usermode through the rhproxy driver. I2C0 and I2C2 are
reserved by the GPU firmware.
Some limitations are:

 - Due to hardware limitations, does not support arbitrary sequences. Only
   Write-Read sequences are supported.
 - Due to hardware limitations, does not support `IOCTL_SPB_LOCK_CONTROLLER`
   and `IOCTL_SPB_UNLOCK_CONTROLLER`.
 - Due to the hardware bug described [here](https://github.com/raspberrypi/linux/issues/254),
   Raspberry Pi cannot communicate reliably with slave devices that do clock 
   stretching, including Atmel ATMEGA microcontrollers. It is recommended to use
   UART to communicate with ATMEGA microcontrollers.


## Registry Settings

The driver supports the following registry settings which can be used to change
the default behavior of the driver.

### Under key `HKLM\System\CurrentControlSet\enum\ACPI\BCM2841\1\Device Parameters`

| Registry Value Name | Type      | Description                                                                                                                                                                                                                                                                                                                                                                                                  |
|---------------------|-----------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| ClockStretchTimeout | REG_DWORD | The value used to program the TOUT (Clock Stretch Timeout) register. This is the maximum amount of time, in SCL clock cycles, that a slave address can stretch the clock before the master fails the transfer with a clock stretch timeout error.  A value of 0 turns off clock stretch timeout detection, allowing slave devices to stretch the clock indefinitely. Value must be in the range [0, 0xffff]. |

