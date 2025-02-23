
obj-y			+= crt0.o
obj-y			+= monitor.o
obj-y			+= vectors.o
obj-$(CONFIG_TTY_68681)	+= tty_68681.o

libex-y			+= src/libc/libc-$(SRCARCH)-none.a
always-y		+= monitor.elf monitor.bin monitor.load

ccflags-y		+= $(firmware-ccflags-y)
ldflags-y 		+= -Wl,--entry=_start -Tsrc/monitor/monitor.ld

# TODO [2025-02-20] this is because the flash programming code raises a warning 
# (gcc-14): "array subscript 0 is outside array bounds of ‘volatile uint8_t[0]’"
ccflags-y		+= -Warray-bounds=0

