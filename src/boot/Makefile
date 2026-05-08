
obj-y			+= boot.o

always-y		+= boot.bin boot.load
libex-y			+= src/libc/libc-$(SRCARCH)-none.a
CFLAGS			+= -Isrc/libc/arch/$(SRCARCH)/include

ccflags-y		+= -Wl,--entry=start -Ttext=0x0000
ccflags-y		+= $(firmware-ccflags-y)

