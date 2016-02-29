
#include <stddef.h>
#include <stdarg.h>
#define WIN9X_COMPAT_SPINLOCK
#include "ntddk.h"
#include <wdf.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <initguid.h>
#include <devpkey.h>
#include "ntddser.h"
#include <wmilib.h>
#include <initguid.h> // required for GUID definitions
#include <wmidata.h>
#include "serial.h"
#include "serialp.h"
#include "serlog.h"
#include "log.h"
#include "trace.h"

// macros for higher speed serial port masks not included in ntddser.h

#define SERIAL_BAUD_230400       ((ULONG)0x00080000)
#define SERIAL_BAUD_460800       ((ULONG)0x00100000)
#define SERIAL_BAUD_921600       ((ULONG)0x00200000)
