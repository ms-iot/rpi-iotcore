Raspberry Pi Board Support Package for Windows 10 IoT Core
==============

## Welcome to the Raspberry Pi Board Support Package (BSP) for Windows 10 IoT Core

This repository contains BSP components for the Raspberry Pi 2, 3, and Compute Module. This BSP repository is under community support; it is functional with the Fall 2018 release of Windows 10 IoT Core but is not actively maintained by Microsoft. BSP elements included in this repository may contain features that are not available with Windows 10 IoT Core releases.

For more information about Windows 10 IoT Core, see online documentation [here](http://windowsondevices.com)

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Firmware binaries

Sample binaries of the firmware is included in [RPi.BootFirmware](bspfiles/Packages/RPi.BootFirmware) to enable quick prototyping. The sources for these binaries are listed below.

1. Firmware binaries : [RaspberryPi/Firmware](https://github.com/raspberrypi/firmware)
2. UEFI Sources : [RPi/UEFI](https://github.com/ms-iot/RPi-UEFI)

### UEFI Customisations

[SMBIOS requirements of Windows 10 IoT Core OEM Licensing](https://docs.microsoft.com/en-us/windows-hardware/manufacture/iot/license-requirements#smbios-support) requires a custom version of kernel.img file with the proper SMBIOS values.

See [PlatformSmbiosDxe.c](https://github.com/ms-iot/RPi-UEFI/blob/ms-iot/Pi3BoardPkg/Drivers/PlatformSmbiosDxe/PlatformSmbiosDxe.c) to update the SMBIOS data. Steps to build the kernel.img is provided in the [RPi/UEFI Github](https://github.com/ms-iot/RPi-UEFI).

## Build the drivers

1. Clone https://github.com/ms-iot/rpi-iotcore
1. Open Visual Studio with Administrator privileges
1. In Visual Studio: File -> Open -> Project/Solution -> Select `rpi-iotcore\build\bcm2836\buildbcm2836.sln`
1. Set your build configuration (Release or Debug)
1. Build -> Build Solution

The resulting driver binaries will be located in the `rpi-iotcore\build\bcm2836\ARM` folder.

## Export the bsp

We provide a `binexport.ps1` script to scrape the BSP components together into a zip file for easy use with the IoT ADK AddonKit.
1. Open Powershell
2. Navigate to rpi-iotcore\tools
3. Run `binexport.ps1` with the appropriate arguments.
    ```powershell
    .\binexport.ps1 C:\Release
    (or)
    .\binexport.ps1 C:\Release -IsDebug # for debug binaries
    ```
4. The script will generate a zip file **RPi_BSP_xx.zip** that can be imported into the IoT-ADK-Addonkit shell using [Import-IoTBSP](https://github.com/ms-iot/iot-adk-addonkit/blob/master/Tools/IoTCoreImaging/Docs/Import-IoTBSP.md).
    ```powershell
    Import-IoTBSP RPi C:\Temp\RPi_BSP_xx.zip
    ```
