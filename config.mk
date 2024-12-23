
# Configuration

BOARD = k30
#BOARD = 68k

# End of Configuration

ifeq ($(BOARD), k30)
#CFLAGS_ARCH = -m68030 -msoft-float
#ASFLAGS_ARCH = -m68030 --defsym __mc68010__=1
CFLAGS_ARCH = -m68010 -DBOARD_K30 -D=__mc68010__
ASFLAGS_ARCH = -m68010 -D=__mc68010__
else
CFLAGS_ARCH = -m68010 -D=__mc68010__
ASFLAGS_ARCH = -m68010 -D=__mc68010__
endif

BUILD = build
AS = /media/work/contrib/llvm-project/build/bin/clang -target m68k-unknown-linux-gnu
AR = /media/work/contrib/llvm-project/build/bin/llvm-ar
CC = /media/work/contrib/llvm-project/build/bin/clang -target m68k-unknown-linux-gnu
#AS = m68k-linux-gnu-as
#AR = m68k-linux-gnu-ar
#CC = m68k-linux-gnu-gcc
OBJDUMP = m68k-linux-gnu-objdump
OBJCOPY = m68k-linux-gnu-objcopy
RANLIB = m68k-linux-gnu-ranlib
INCLUDE = $(GLOWORM)/include
ASFLAGS = $(ASFLAGS_ARCH)
CFLAGS = -I$(INCLUDE) $(CFLAGS_ARCH) -DBOARD_$(BOARD) -nostartfiles -nostdlib -fno-zero-initialized-in-bss -g -Os
LDFLAGS = -Wl,--build-id=none

# reduce the elf output size by reducing the alignment
CFLAGS += -z max-page-size=0x10

