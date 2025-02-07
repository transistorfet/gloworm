
# disabled make's built-in rules and built-in variables
MAKEFLAGS += -rR

ifneq ($(O),)
    OUTPUT := $(patsubst %/,%,$(O))/
endif

# TODO building to another directory doesn't work yet
#OUTPUT := build
export OUTPUT

this-makefile := $(lastword $(MAKEFILE_LIST))
src-root := $(realpath $(dir $(this-makefile)))

include $(src-root)/tools/build/Makefile.defaults

-include $(kconfig-file)

build-root	:= $(if $(OUTPUT),$(shell mkdir -p $(OUTPUT) && cd $(OUTPUT) && pwd))
export src-root build-root kconfig-file config-h

PHONY += all
all: generate_config decend

PHONY += config
config:
	# TODO open config program

PHONY += generate_config
generate_config: $(config-h)

$(config-h): $(kconfig-file)
	$(shell mkdir -p $(dir $(config-h)))
	cat $(kconfig-file) | sed '/^\s*#.*CONFIG_/d; s/^\(\s*\)#/\1\/\//; s/=y/=1/; s/\(.*\)=\(.*\)/#define \1 \2/' > $(config-h)

PHONY += decend
decend:
	$(MAKE) -f $(src-root)/tools/build/Makefile.build dir=src


# TODO 128 is just barely enough for 20 commands, kernel, devfiles
#BLOCKS = 128
BLOCKS := 20480
IMAGE := minix-build.img
LOOPBACK := /dev/loop8
#MOUNTPOINT := $(OUTPUT)image
MOUNTPOINT := build/image
SUDO := sudo

PHONY += create-image build-image-files mount-image umount-image

build-disk-image: decend create-image-dir kernelfile commandfiles devicefiles otherfiles

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
	$(SUDO) cp $(OUTPUT)src/kernel.bin $(MOUNTPOINT)

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
	find . \( -name "*.o" -or -name "*.a" -or -name "*.bin" -or -name "*.elf" -or -name "*.load" -or -name "*.send" \) -delete -print

PHONY += FORCE
FORCE:

.PHONY: $(PHONY)
