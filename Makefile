
# disabled make's built-in rules and built-in variables
MAKEFLAGS += -rR

ifneq ($(O),)
    OUTPUT := $(patsubst %/,%,$(O))/
endif
export OUTPUT

this-makefile := $(lastword $(MAKEFILE_LIST))
src-root := $(realpath $(dir $(this-makefile)))

include $(src-root)/tools/build/Makefile.defaults

-include $(kconfig-file)

build-root	:= $(if $(OUTPUT),$(shell mkdir -p $(OUTPUT) && cd $(OUTPUT) && pwd))
export src-root build-root kconfig-file config-h

PHONY += all
all: generate-config decend

PHONY += config
config:
	kconfig-conf $(src-root)/Kconfig

PHONY += menuconfig
menuconfig:
	kconfig-mconf $(src-root)/Kconfig

PHONY += dockerconfig
dconfig:
	cd tools/config && ./run.sh

PHONY += generate-config
generate-config: $(config-h)

$(config-h): $(kconfig-file)
	$(shell mkdir -p $(dir $(config-h)))
	cat $(kconfig-file) | sed '/^\s*#.*CONFIG_/d; s/^\(\s*\)#/\1\/\//; s/=y/=1/; s/\(.*\)=\(.*\)/#define \1 \2/' > $(config-h)

PHONY += decend
decend: generate-config
	$(MAKE) -f $(src-root)/tools/build/Makefile.build dir=src

src/%: generate-config
	$(MAKE) -f $(src-root)/tools/build/Makefile.build dir=$(patsubst %/,%,$(dir $@)) $@

# Build the image that can be written to the 68kSupervisor of computie
output.txt: $(OUTPUT)src/monitor/monitor.bin
	hexdump -v -e '/1 "0x%02X, "' $@ > output.txt

# Convenience targets
PHONY += monitor.load monitor.bin monitor.elf kernel.load kernel.bin kernel.elf
monitor.load: src/monitor/monitor.load
monitor.bin: src/monitor/monitor.bin
monitor.elf: src/monitor/monitor.elf
kernel.load: src/kernel/kernel.load
kernel.bin: src/kernel/kernel.bin
kernel.elf: src/kernel/kernel.elf

# Test building and running targets
tests: generate-config
	#$(MAKE) -f $(src-root)/tools/build/Makefile.test dir=tests tests
	$(MAKE) -f $(src-root)/tools/build/Makefile.build dir=tests tests

bare-tests: generate-config
	#$(MAKE) -f $(src-root)/tools/build/Makefile.test dir=tests bare-tests
	$(MAKE) -f $(src-root)/tools/build/Makefile.build dir=tests bare-tests


# TODO 128 is just barely enough for 20 commands, kernel, devfiles
#BLOCKS = 128
BLOCKS := 20480
IMAGE := minix-build.img
LOOPBACK := /dev/loop8
#MOUNTPOINT := $(OUTPUT)image
MOUNTPOINT := build/image
SUDO := sudo

PHONY += create-image build-image-files mount-image umount-image

build-image: decend create-image-dir kernelfile commandfiles devicefiles otherfiles

create-image-dir:
	mkdir -p $(MOUNTPOINT)

create-image:
	dd if=/dev/zero of=$(IMAGE) bs=1K count=$(BLOCKS)
	$(SUDO) losetup $(LOOPBACK) $(IMAGE)
	$(SUDO) mkfs.minix -1 -n 14 $(LOOPBACK) $(BLOCKS)
	$(SUDO) losetup -d $(LOOPBACK)

mount-image:
	$(SUDO) losetup $(LOOPBACK) $(IMAGE)
	$(SUDO) mount -t minix $(LOOPBACK) $(MOUNTPOINT)

umount-image:
	$(SUDO) umount $(LOOPBACK)
	$(SUDO) losetup -d $(LOOPBACK)

kernelfile:
	$(SUDO) cp $(OUTPUT)src/kernel/kernel.bin $(MOUNTPOINT)

commandfiles:
	$(SUDO) mkdir -p $(MOUNTPOINT)/bin
	$(MAKE) -f $(src-root)/tools/build/Makefile.build dir=src/commands SUDO=$(SUDO) LOCATION=$(MOUNTPOINT)/bin copy-commands

devicefiles:
	$(SUDO) mkdir -p $(MOUNTPOINT)/dev
	$(SUDO) mknod $(MOUNTPOINT)/dev/tty0 c 2 0
	$(SUDO) mknod $(MOUNTPOINT)/dev/tty1 c 2 1
	$(SUDO) mknod $(MOUNTPOINT)/dev/mem0 c 3 0
	$(SUDO) mknod $(MOUNTPOINT)/dev/ata0 c 4 0

otherfiles:
	$(SUDO) mkdir -p $(MOUNTPOINT)/etc
	$(SUDO) mkdir -p $(MOUNTPOINT)/proc
	$(SUDO) mkdir -p $(MOUNTPOINT)/home
	$(SUDO) mkdir -p $(MOUNTPOINT)/media
	$(SUDO) cp -r etc/* $(MOUNTPOINT)/etc


# TODO need a better clean rule
clean:
	rm -f $(config-h)
	#ifneq ($(OUTPUT),)
	#	rm -f $(OUTPUT)
	#endif
	find src/ $(OUTPUT) \( -name "*.o" -or -name "*.d" -or -name "*.a" -or -name "*.bin" -or -name "*.elf" -or -name "*.load" -or -name "*.send" \) -delete -print


PHONY += help
help:
	@echo  'Cleaning targets:'
	@echo  '  clean            - Remove most generated files but keep the config'
	@echo  ''
	@echo  'Configuration targets:'
	@echo  '  config        - Update current config using a terminal program'
	@echo  '  menuconfig    - Update current config using a ncurses menu'
	@echo  '  dockerconfig  - Update current config using a ncurses menu inside a'
	@echo  '                  docker container, if you do not have the'
	@echo  '                  `kconfig-frontends` debian package installed'
	@echo  ''
	@echo  'Other generic targets:'
	@echo  '  all           - Build all targets marked with [*]'
	@echo  '* kernel.bin    - Build the kernel'
	@echo  '* monitor.bin   - Build the monitor (loaded into the ROM)'
	@echo  '* src/commands  - Build all the user programs'
	@echo  '  src/dir/      - Build all files in dir and below'
	@echo  '  output.txt    - Build the file included by 68kSupervisor to boot from'
	@echo  '                  the arduino'
	@echo  ''
	@echo  'Test targets:'
	@echo  '  tests         - Run all the tests that can run on the host machine'
	@echo  '  bare-tests    - Build all the baremetal tests that run on the target machine'
	@echo  ''
	@echo  'Disk image targets:'
	@echo  '  create-image  - Create a new disk image file initialized with minix1 fs'
	@echo  '  mount-image   - Mount the disk image file to the default location'
	@echo  '  umount-image  - Unmount the disk image file'
	@echo  '  build-image   - Build `all` and copy the kernel, commands, /etc, and /dev'
	@echo  '                  to the image mountpoint'


PHONY += FORCE
FORCE:

.PHONY: $(PHONY)
