# Freescale i.MX6 Series GPIO Client Driver

* Currently the driver supports only Dual/Quad variant of i.MX6
* Define IMX6DQ (default) to compile driver for i.MX6 Dual/Quad

## Supporting a new i.MX6 Variant like SoloLite
* Replace IMX6DQ preprocessor defined in proj properties with IMX6SL
* On compiling, you will get errors with messages for code sections that require porting
* Write missing definitions for SoloLite using #elseif to extend the #ifdef and recompile