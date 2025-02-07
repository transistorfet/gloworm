
obj-y				+= start.o
obj-y				+= main.o
obj-y				+= syscall_entry.o
obj-y				+= syscall.o
obj-y				+= interrupts.o
obj-y				+= bh.o
obj-y				+= time.o
obj-y				+= driver.o

obj-y				+= mm/
obj-y				+= proc/

obj-y				+= drivers/
obj-y				+= fs/
obj-$(CONFIG_NET)		+= net/

obj-$(CONFIG_PSEUDOSHELL)	+= cmd-init.o
obj-$(CONFIG_PSEUDOSHELL)	+= cmd-sh.o

libex-y				+= src/libc/libc-68k-os.a

ldflags-y 			+= -Wl,--entry=_start -Tsrc/kernel/kernel.ld
ccflags-$(CONFIG_PSEUDOSHELL)	+= -DONEBINARY

# Recompile the init and sh user programs into a version without a main() that
# can be linked into the kernel, to run sh without a working disk drive
$(objprefix)cmd-init.o: src/commands/init.c
	$(cmd_cc_o)
$(objprefix)cmd-sh.o: src/commands/sh.c
	$(cmd_cc_o)

