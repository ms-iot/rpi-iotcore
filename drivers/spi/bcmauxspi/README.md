# Raspberry Pi 2 (BCM2836) AUXSPI Controller driver for SPB Framework

This is an SPI driver for the AUXSPI peripheral. There are two AUXSPI
peripherals located on the auxiliary block. Although the hardware FIFO is only
4 items deep, each entry is 32 bits wide, so we can pack up to 4 bytes
per entry yeilding an effective depth of 16 bytes. The hardware also supports
a variable shift length mode which we can use to handle non multiple of 4
transfer lengths. This driver has undergone data integrity testing over all
4 SPI modes and a variety of transfer lengths and clock speeds.
