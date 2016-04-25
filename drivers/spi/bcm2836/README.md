# Raspberry Pi 2 (BCM2836) SPI Controller driver for SPB Framework

This is an SPI driver for the SPI peripheral. In addition to the two AUXSPI
controllers on the AUX block, there is a single SPI peripheral (SPI0). This driver
is implemented as an [SpbCx Controller Driver](https://msdn.microsoft.com/en-us/library/windows/hardware/hh406203(v=vs.85).aspx).
SPI0 is exposed to usermode by the rhproxy driver.
