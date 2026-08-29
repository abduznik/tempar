#ifndef COMMON_H
#define COMMON_H

// Workaround: newer PSPSDK pspmodulemgr_kernel.h needs SceLoadCoreExecFileInfo
// which is defined in psploadcore.h but that header isn't always included first
typedef struct {} SceLoadCoreExecFileInfo;

#include "cheat.h"
#include "config.h"
#include "ctrl.h"
#include "disasm.h"
#include "filebrowser.h"
#include "filebuffer.h"
#include "float.h"
#include "kmalloc.h"
#include "language.h"
#include "log.h"
#include "main.h"
#include "mem.h"
#include "menu.h"
#include "psid.h"
#include "pspdebugkb.h"
#include "psploadcore.h"
#include "screenshot.h"
#include "search.h"
#include "syslibc.h"
#include "text.h"
#include "usb.h"
#include "utils.h"
#include "version.h"

#endif