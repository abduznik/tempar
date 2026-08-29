#ifndef PSP_COMPAT_H
#define PSP_COMPAT_H

/*
 * Compatibility shim for newer PSPSDK versions.
 *
 * pspmodulemgr_kernel.h (pulled in by pspkernel.h) references
 * SceLoadCoreExecFileInfo on line 51, but the Docker image's SDK
 * does not define it in any header that's included before use.
 *
 * This provides the typedef so compilation succeeds.
 * Include this header BEFORE any PSPSDK kernel headers.
 */

typedef struct { int _dummy; } SceLoadCoreExecFileInfo;

#endif
