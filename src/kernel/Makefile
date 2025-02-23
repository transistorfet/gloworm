
obj-y				+= main.o
obj-y				+= syscall.o
obj-y				+= bh.o
obj-y				+= time.o
obj-y				+= driver.o

obj-y				+= mm/
obj-y				+= proc/

obj-y				+= arch/
obj-y				+= drivers/
obj-y				+= fs/
obj-$(CONFIG_NET)		+= net/

obj-$(CONFIG_SHELL_IN_KERNEL)	+= cmd-init.o
obj-$(CONFIG_SHELL_IN_KERNEL)	+= cmd-sh.o

libex-y				+= src/libc/libc-$(SRCARCH)-os.a
always-y			+= kernel.elf kernel.bin kernel.load

ldflags-y 			+= -Wl,--entry=_start -Tsrc/kernel/arch/$(SRCARCH)/kernel.ld

# Set these flags for all subdirectories instead of just the current Makefile
CFLAGS				+= $(kernel-ccflags-y)
CFLAGS				+= -Isrc/kernel/arch/$(SRCARCH)/include

# Recompile the init and sh user programs into a version without a main() that
# can be linked into the kernel, to run sh without a working disk drive
$(objprefix)cmd-sh.o $(objprefix)cmd-init.o: CFLAGS += -DIN_KERNEL
$(objprefix)cmd-init.o: src/commands/init.c
	$(cmd_cc_o)
$(objprefix)cmd-sh.o: src/commands/sh.c src/commands/echo.c src/commands/cat.c src/commands/ls.c src/commands/mkdir.c src/commands/pwd.c src/commands/mv.c src/commands/cp.c src/commands/ln.c src/commands/rm.c src/commands/chmod.c src/commands/date.c src/commands/ps.c src/commands/kill.c src/commands/mount.c src/commands/umount.c src/commands/sync.c
	$(cmd_cc_o)

