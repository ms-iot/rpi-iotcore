# Raspberry Pi VCOS Win32 Kernel Mode Export Driver

This kernel mode export driver is the equivalent of the vcos module that is
available in userland. Just like the vcos module in userland vcos_win32_kern
acts as the OS abstraction layer proving Windows kernel mode interfaces.Not
all of vcos_* interface is implemented for kernel mode.