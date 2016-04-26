# Raspberry Pi 2 (BCM2836) GPIO Client Driver

This is the GPIO driver for Raspberry Pi. It implements GPIO input/output,
interrupts, and pin muxing. It is a GpioClx client driver. A subset of pins
is exposed to usermode through the rhproxy driver. For more information on
the GpioClx framework, see
[General-Purpose I/O (GPIO) Driver Reference](https://msdn.microsoft.com/en-us/library/windows/hardware/hh439515(v=vs.85).aspx).