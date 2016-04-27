# Raspberry Pi 2 (BCM2836) VC4 Mailbox Driver

The RPIQ driver provides mailbox inteface to the VC GPU firmware. The mailbox
interface is defined by Raspberry Pi and documentation can be found here
https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface. The
rpiq.h header provides some basic property definition.

Currently the VCHIQ driver would use the mailbox interface by sending an IOCTL
to RPIQ to intiialize the VCHIQ shared memory interface with the firmware. The
RPIQ driver also is responsible to setup the mac address for Pi platform during
boot.