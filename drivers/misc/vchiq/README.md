# Raspberry Pi VCHIQ Driver

This is the Video Core Host Interface Queue driver that is responsible to
marshall communication with the GPU firmware. THe VCHIQ driver is main function
is to provide the interface with firmware with various client such as MMAL.

VCHIQ communicates with the GPU firmware through a shared memory that is
initialized through mailbox interface (rpiq). The shared memory are divided
into slots. The first slot (slot 0) acts like a header containing the structer
definition for the rest of the slot. The remaining slots are allocated for The
master (firmware) and slave (VCHIQ). Firmware would use the master slots to
send message to slave while VCHIQ driver would use the slave slot to send
message to the firmware. Firmware and VCHIQ notifies the avaibility of new
message through doorbell interrups. A portion of the shared memory at the end
of the shared memory is called fragment block. The fragment block is meant to
patch DMA transfer for buffer that is not align with cache size. This is not
used as VCHIQ takes advantage of DMA api for DMA buffer.