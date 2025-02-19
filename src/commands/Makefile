
commands-y		:=

commands-y		+= cat
commands-y		+= chmod
commands-y		+= cp
commands-y		+= date
commands-y		+= echo
commands-y		+= hello
commands-y		+= ifconfig
commands-y		+= init
commands-y		+= kill
commands-y		+= ln
commands-y		+= ls
commands-y		+= mkdir
commands-y		+= mount
commands-y		+= mv
commands-y		+= ns
commands-y		+= ntpdate
commands-y		+= ps
commands-y		+= pwd
commands-y		+= rm
commands-y		+= sh
commands-y		+= sync
commands-y		+= tar
commands-y		+= umount
commands-y		+= test
commands-y		+= nettest
commands-y		+= tcptest
commands-y		+= tcpserv
commands-y		+= httpd
commands-y		+= daemontest

#userprogs		:= $(commands-y)
#userprogs		+= $(foreach name,$(commands-y),$(name).send)
userprogs		+= $(foreach name,$(commands-y),$(name).elf)
userprogs-stripped	+= $(addprefix $(objprefix),$(foreach name,$(commands-y),$(name).strip))

libex-y			+= src/libc/libc-68k-os.a
extra-y			+= echo.c cat.c ls.c mkdir.c pwd.c mv.c cp.c ln.c rm.c chmod.c date.c ps.c kill.c mount.c umount.c sync.c

# When other utilities are included into the shell, this flag will tell them not to include a main function
$(objprefix)sh.o: CFLAGS += -DIN_SHELL

# Strip and copy the user programs to the image
userprogs-bare := $(addprefix $(LOCATION)/,$(commands-y))

copy-commands: $(userprogs-bare)

$(LOCATION)/%: $(objprefix)%.strip
	$(SUDO) cp $< $(LOCATION)/$(notdir $@)

