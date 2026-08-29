#ifndef PSP_COMPAT_H
#define PSP_COMPAT_H

/*
 * Compatibility shim for newer PSPSDK versions.
 *
 * The latest PSPSDK's pspmodulemgr_kernel.h references
 * SceLoadCoreExecFileInfo which is defined in psploadcore.h,
 * but not all translation units include psploadcore.h before
 * pspkernel.h. This stub prevents the "unknown type" error.
 *
 * Include this header BEFORE any PSPSDK kernel headers.
 */

typedef struct {} SceLoadCoreExecFileInfo;

#endif
