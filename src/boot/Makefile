
obj-y			+= boot.o

libex-y			+= src/libc/libc-$(SRCARCH)-none.a
always-y		+= boot.bin boot.load

ccflags-y		+= -Wl,--entry=start -Ttext=0x0000
ccflags-y		+= $(firmware-ccflags-y)

