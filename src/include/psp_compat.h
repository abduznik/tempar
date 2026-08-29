#ifndef PSP_COMPAT_H
#define PSP_COMPAT_H

/*
 * Compatibility shim for newer PSPSDK versions.
 *
 * pspmodulemgr_kernel.h references SceLoadCoreExecFileInfo
 * which is defined in psploadcore.h, but not all translation
 * units include psploadcore.h before pspkernel.h.
 *
 * This header provides the forward declaration early.
 * Include BEFORE any PSPSDK kernel headers.
 */

#include <psploadcore.h>

#endif
