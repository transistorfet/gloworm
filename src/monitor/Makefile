
obj-y			+= crt0.o
obj-y			+= monitor.o
obj-y			+= vectors.o
obj-$(CONFIG_TTY_68681)	+= tty_68681.o

libex-y			+= src/libc/libc-68k-none.a

ldflags-y 		+= -Wl,--entry=_start -Tsrc/monitor/monitor.ld
ccflags-y 		+= -mpcrel

always-y		+= monitor.elf monitor.bin monitor.load

