Raspberry Pi Board Support Package for Windows 10 IoT Core
==============

## Welcome to the Raspberry Pi Board Support Package (BSP) for Windows 10 IoT Core

This repository contains BSP components for the Raspberry Pi 2, 3, and Compute Module. This BSP repository is under community support; it is functional with the Fall 2018 release of Windows 10 IoT Core but is not actively maintained by Microsoft. BSP elements included in this repository may contain features that are not available with Windows 10 IoT Core releases.

For more information about Windows 10 IoT Core, see online documentation [here](http://windowsondevices.com)

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Building the BSP

1. Clone https://github.com/ms-iot/rpi-iotcore
1. Open Visual Studio with Administrator privileges
1. In Visual Studio: File -> Open -> Project/Solution -> Select `rpi-iotcore\build\bcm2836\buildbcm2836.sln`
1. Set your build configuration (Release or Debug)
1. Build -> Build Solution

The resulting driver binaries will be located in the `rpi-iotcore\build\bcm2836\ARM\Release` folder.

We provide a `binexport.cmd` script to scrape the BSP components together into one folder for easy use with the IoT ADK AddonKit.
1. Open command prompt
1. Navigate to rpi-iotcore\tools
1. Run `binexport.cmd` with the appropriate arguments. See [usage](tools/binexport.cmd) for more details.