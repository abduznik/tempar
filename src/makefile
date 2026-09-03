# Consolidated PSP plugin build (was makefile_psp / makefile_lite).
# Usage: make -C src -f makefile VARIANT=psp|lite   (default: psp)
# The old makefile_psp / makefile_lite names remain as thin wrappers.

VARIANT ?= psp

ifeq ($(VARIANT),lite)
TARGET = tempar_lite
else
TARGET = tempar
endif

OBJS = imports.o \
       objects/main.o \
       objects/config.o \
       objects/language.o \
       objects/filebuffer.o \
       objects/cheat.o \
       objects/psid.o \
       objects/menu.o \
       objects/search.o \
       objects/log.o \
       objects/utils.o \
       objects/pspdebugkb.o \
       objects/syslibc.o \
       objects/screenshot.o \
       objects/kmalloc.o \
       objects/ctrl.o \
       objects/disasm.o \
       objects/text.o \
       objects/float.o \
       objects/usb.o \
       objects/sdk.o \
       objects/filebrowser.o

INCDIR = include

# common feature set, then the full build adds the heavy extras
DEFS = -D_FONT_acorn -D_CWCHEAT_ -D_PSPAR_ -D_GUIDE_ -D_MIPS_ -D_AUTOOFF_ -D_MULTILANGUAGE_
ifeq ($(VARIANT),psp)
DEFS += -D_USB_ -D_PSID_ -D_SCREENSHOT_ -D_UMDDUMP_ -D_MODLIST_ -D_THLIST_
endif
ifeq ($(VARIANT),lite)
DEFS += -D_LITE_
endif

CFLAGS = -Os -G0 -Wall -fshort-wchar -fno-pic -mno-check-zero-division \
         -Wno-implicit-function-declaration -Wno-incompatible-pointer-types \
         -Wno-int-conversion -Wno-int-to-pointer-cast $(DEFS)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1

LIBDIR = libs
LDFLAGS = -nostartfiles
LIBS = -lpspdebug -lpsppower_driver -lpspreg_driver -lpspge_driver -lpsprtc

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
