# RaspberryPi SD2.0 Host Controller Driver
This is a Secure Digital (SD) Host Controller miniport ARM driver for a Broadcom proprietary SD Host Controller referred to in Broadcom BCM2835 datasheet as SDHost. The driver works in conjunction with sdport.sys, which implements SD/SDIO/eMMC protocol and WDM interfaces.
The SDHC interfaces to SD memory cards compliant to SD Memory Card Specifications version 2.0 dated May 2006, while this SD Host Controller itself is not SD standard compliant.

This SDHC is available on RPi2 and RPi3, however it is currently enabled only on RPi3 for SD exclusively. Switching between ArasanSD and this SDHC for SD storage is not possible without UEFI firmware changes.

## Workarounds
Workarounds had to be implemented to fit this non-standard SDHC into Sdhost port/miniport framework, while following the standard SDHC implementation as a guidance. You can refer to Microsoft published standard SDHC sample here: https://github.com/Microsoft/Windows-driver-samples/tree/master/sd/miniport/sdhc.
Workarounds used across the implementation source code are explained inline and are prefixed with the word 'WORKAROUND'.

## SDHC Hardware Limitations and Pitfalls
1. Not SD2.0 Host Controller compliant.
2. Non-standard registers layout.
2. Requires a lot of CPU overhead on driver level to overcome its hardware limitations.
3. It is not a  DMA bus-master i.e no ADMA.
4. Interrupt sources are not well chosen. e.g. no interrupt for command completion, only BUSY interrupt for those commands with busy response (e.g. R1b).

## ArasanSD vs RaspberryPi SD
We will refer to the bcm2386sdhc.sys driver as ArasanSD and rpisdhc.sys as RaspberryPi SD

Pros/Cons | ArasanSD | RaspberryPi SD
----------|----------|---------------
Pros | Low CPU utilization | Crashdump support, Higher read/write transfer rate (Up to 17MB/s read, and 7MB/s write)
Cons | Consume alot of DPCs causing system to choke on heavy load  | High CPU utilization

## Testing
The SDHC driver rpisdhc.sys has passed HLK tests for SD Class Storage. SDCards known to work: SanDisk 8GB Class4, SanDisk 16GB Class10, Samsung 32GB/16GB EVO Class10, Transcend 32GB Class10.

Note: Many SDCard brands/models than the ones mentioned have been tested and are all known to work, no single SDCard had problem to boot Windows10 IoT Core and functional correctly on RPi3.

#### Benchmarking Tool
A performance comparison between ArasanSD and this SDHC was performed during RS1 development phase using Diskspd: https://gallery.technet.microsoft.com/DiskSpd-a-robust-storage-6cd2f223.

```
> diskspd -c2G -w50 -b4K -t4 -s4b -T1b -o8 -d60 -h testfile.dat > 4KW50.txt
> diskspd -c2G -w0 -b64K -t4 -s4b -T1b -o8 -d60 -h testfile.dat > 64KW0.txt
> diskspd -c2G -w100 -b64K -t4 -s4b -T1b -o8 -d60 -h testfile.dat > 64KW100.txt
> diskspd -c2G -w0 -b256K -t4 -s4b -T1b -o8 -d60 -h testfile.dat > 256KW0.txt
> diskspd -c2G -w100 -b256K -t4 -s4b -T1b -o8 -d60 -h testfile.dat > 256KW100.txt
> diskspd -c2G -w0 -b1M -t1 -s4b -o1 -d60 -h testfile.dat > 1MW0.txt
> diskspd -c2G -w100 -b1M -t1 -s4b -o1 -d10 -h testfile.dat > 1MW100.txt
```
