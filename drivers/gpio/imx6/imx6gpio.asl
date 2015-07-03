//
// To compile this file to ACPITABL.dat, run makeacpi.cmd from a razzle prompt.
// Copy ACPITABL.dat to %windir%\system32, turn on testsigning, and reboot.
//

DefinitionBlock ("ACPITABL.dat", "SSDT", 1, "MSFT", "GPI0", 1)
{
    Scope (\_SB)
    {
        Device (GPI0)
        {
            Name (_HID, "IMX6GPIO")
            Name (_UID, 0x0)
            Method (_STA)
            {
                Return(0xf)
            }
            Method (_CRS, 0x0, NotSerialized) {
                Name (RBUF, ResourceTemplate () {
                    // IOMUXC Registers
                    MEMORY32FIXED(ReadWrite, 0x020E0000, 0x4000, )
                    // GPIO Registers
                    MEMORY32FIXED(ReadWrite, 0x0209C000, 0x4000, )
                })
                Return(RBUF)
            }
        }

        //
        // RHProxy Device Node to enable WinRT API
        //

        Device(RHPX)
        {
            Name(_HID, "MSFT8000")
            Name(_CID, "MSFT8000")
            Name(_UID, 1)

            Name(_CRS, ResourceTemplate()
            {
                // GPIO1_IO01
                GpioIO(Shared, PullUp, 0, 0, IoRestrictionNone, "\\_SB.GPI0", 0, ResourceConsumer, , ) { 1 }
                GpioInt(Edge, ActiveBoth, Shared, PullUp, 0, "\\_SB.GPI0",) { 1 }

                // GPIO1_IO02
                GpioIO(Shared, PullUp, 0, 0, IoRestrictionNone, "\\_SB.GPI0", 0, ResourceConsumer, , ) { 2 }
                GpioInt(Edge, ActiveBoth, Shared, PullUp, 0, "\\_SB.GPI0",) { 2 }

                // GPIO1_IO03
                GpioIO(Shared, PullUp, 0, 0, IoRestrictionNone, "\\_SB.GPI0", 0, ResourceConsumer, , ) { 3 }
                GpioInt(Edge, ActiveBoth, Shared, PullUp, 0, "\\_SB.GPI0",) { 3 }
                
                // GPIO1_IO04
                GpioIO(Shared, PullUp, 0, 0, IoRestrictionNone, "\\_SB.GPI0", 0, ResourceConsumer, , ) { 4 }
                GpioInt(Edge, ActiveBoth, Shared, PullUp, 0, "\\_SB.GPI0",) { 4 }

				// GPIO1_IO05
                GpioIO(Shared, PullUp, 0, 0, IoRestrictionNone, "\\_SB.GPI0", 0, ResourceConsumer, , ) { 5 }
                GpioInt(Edge, ActiveBoth, Shared, PullUp, 0, "\\_SB.GPI0",) { 5 }

				// GPIO1_IO06
                GpioIO(Shared, PullUp, 0, 0, IoRestrictionNone, "\\_SB.GPI0", 0, ResourceConsumer, , ) { 6 }
                GpioInt(Edge, ActiveBoth, Shared, PullUp, 0, "\\_SB.GPI0",) { 6 }

				// GPIO1_IO07
                GpioIO(Shared, PullUp, 0, 0, IoRestrictionNone, "\\_SB.GPI0", 0, ResourceConsumer, , ) { 7 }
                GpioInt(Edge, ActiveBoth, Shared, PullUp, 0, "\\_SB.GPI0",) { 7 }

				// GPIO1_IO08
                GpioIO(Shared, PullUp, 0, 0, IoRestrictionNone, "\\_SB.GPI0", 0, ResourceConsumer, , ) { 8 }
                GpioInt(Edge, ActiveBoth, Shared, PullUp, 0, "\\_SB.GPI0",) { 8 }
            })
    
            Name(_DSD, Package()
            {
                ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package()
                {
                    // GPIO Pin Count and supported drive modes
                    Package (2) { "GPIO-PinCount", 9 },
                    Package (2) { "GPIO-UseDescriptorPinNumbers", 1 },
                    // InputHighImpedance, InputPullUp, InputPullDown, OutputCmos
                    Package (2) { "GPIO-SupportedDriveModes", 0xf },
                }
            })
        }
    }
}