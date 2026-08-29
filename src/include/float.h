#ifndef FLOAT_H
#define FLOAT_H

#include <pspkerneltypes.h>

#define MODE_GENERIC 0
#define MODE_EXP 1
#define MODE_FLOAT_ONLY 2

void f_cvt(u32 *address, char *buf, int bufsize, int precision, int mode);

#endif