
# disabled make's built-in rules and built-in variables
MAKEFLAGS += -rR

ifneq ($(O),)
    OUTPUT := $(patsubst %/,%,$(O))/
endif
export OUTPUT C

this-makefile	:= $(lastword $(MAKEFILE_LIST))
src-root	:= $(realpath $(dir $(this-makefile)))
kconfig-file	:= $(if $(C),$(src-root)/$(C),$(src-root)/.config)
export src-root kconfig-file

config-h	:= $(src-root)/include/generated/config.h


PHONY += all
all: decend

PHONY += config
config:
	KCONFIG_CONFIG=$(kconfig-file) kconfig-conf $(src-root)/Kconfig

PHONY += dockerconfig
dockerconfig:
	cd tools/config && KCONFIG_CONFIG=$(kconfig-file) ./configure.sh

PHONY += menuconfig
menuconfig:
	@if [ $(from) ]; then									\
		if [ -f "$(kconfig-file)" && ! "$(overwrite)" ]; then				\
			echo "WARNING: a config named $(kconfig-file) already exists. Use the overwrite=y flag to copy anyways";	\
		else										\
			mkdir -p $(dir $(kconfig-file));					\
			cp $(from) $(kconfig-file);						\
			KCONFIG_CONFIG=$(kconfig-file) kconfig-mconf $(src-root)/Kconfig;	\
		fi										\
	else											\
		KCONFIG_CONFIG=$(kconfig-file) kconfig-mconf $(src-root)/Kconfig;		\
	fi

PHONY += oldconfig
oldconfig:
	KCONFIG_CONFIG=$(kconfig-file) kconfig-conf --oldconfig $(src-root)/Kconfig

PHONY += olddefconfig
olddefconfig: FORCE
	@if [ -f "$(kconfig-file)" ] && [ ! "$(overwrite)" ]; then					\
		echo "WARNING: a config named $(kconfig-file) already exists. Use the overwrite=y flag to copy anyways";	\
	else											\
		if [ "$(from)" ]; then								\
			mkdir -p $(dir $(kconfig-file));					\
			cp $(from) $(kconfig-file);						\
		fi;										\
		KCONFIG_CONFIG=$(kconfig-file) kconfig-conf --olddefconfig $(src-root)/Kconfig;	\
	fi

PHONY += defaults.config
defaults.config:
	KCONFIG_CONFIG=$@ kconfig-conf --alldefconfig $(src-root)/Kconfig

PHONY += decend
decend:
	$(MAKE) -f $(src-root)/tools/build/Makefile.build dir=src

# Build the image that can be written to the 68kSupervisor of computie
output.txt: $(OUTPUT)src/monitor/monitor.bin
	hexdump -v -e '/1 "0x%02X, "' $@ > output.txt

# Make it possible to compile individual targets in src and tests
src/% tests/%: FORCE
	$(MAKE) -f $(src-root)/tools/build/Makefile.build dir=$(patsubst %/,%,$(dir $@)) $@

# Convenience targets
PHONY += monitor.load monitor.bin monitor.elf kernel.load kernel.bin kernel.elf
monitor.load: src/monitor/monitor.load
monitor.bin: src/monitor/monitor.bin
monitor.elf: src/monitor/monitor.elf
kernel.load: src/kernel/kernel.load
kernel.bin: src/kernel/kernel.bin
kernel.elf: src/kernel/kernel.elf


# Test building and running targets
PHONY += tests bare-tests
tests bare-tests:
	$(MAKE) -f $(src-root)/tools/build/Makefile.build dir=tests $@


# Diskimage building targets
use-diskimage-utils		:= mount

diskimage-aliases		:=
diskimage-aliases		+= create-diskimage diskimage create-and-build-diskimage
ifeq ($(use-diskimage-utils),mount)
	diskimage-aliases	+= mount-diskimage umount-diskimage mount-and-build-diskimage
else
	diskimage-aliases	+= 
endif

PHONY += $(diskimage-aliases)
$(diskimage-aliases):
	$(MAKE) -f $(src-root)/tools/build/Makefile.$(use-diskimage-utils) $@


# TODO need a better clean rule
clean:
	rm -f $(config-h)
	rm -f src/kernel/arch/*/kernel.ld
	#ifneq ($(OUTPUT),)
	#	rm -f $(OUTPUT)
	#endif
	find $(OUTPUT)src/ $(OUTPUT)tests/ \( -name "*.o" -or -name "*.d" -or -name "*.a" -or -name "*.bin" -or -name "*.elf" -or -name "*.load" -or -name "*.send" \) -delete -print


##############>>--------------------------------------------------------------------------------<< 80 char limit (line wrap at 97 chars)
PHONY += help
help:
	@echo  'Cleaning targets:'
	@echo  '  clean            - Remove most generated files but keep the config'
	@echo  ''
	@echo  'Configuration targets:'
	@echo  '  menuconfig    - Update current config using an ncurses menu'
	@echo  '  olddefconfig  - Update current config by selecting defaults for new values'
	@echo  '  oldconfig     - Update current config by prompting for new values'
	@echo  '  config        - Start new config by prompting for every value'
	@echo  '  dockerconfig  - Update current config using an ncurses menu inside a docker'
	@echo  '                  container (if the `kconfig-frontends` debian package is not'
	@echo  '                  installed'
	@echo  ''
	@echo  '  > Configuration target options:'
	@echo  '    from=<file> - Copy <file> to config location before starting'
	@echo  '    overwrite=y - When using `from`, overwrite the existing config'
	@echo  ''
	@echo  'Build targets:'
	@echo  '  all           - Build all targets marked with [*]'
	@echo  '* kernel.bin    - Build the kernel'
	@echo  '* monitor.bin   - Build the monitor (loaded into the ROM)'
	@echo  '* src/commands  - Build all the user programs'
	@echo  '  src/<dir>/    - Build all files in dir and below'
	@echo  '  output.txt    - Build the file included by 68kSupervisor to boot from'
	@echo  '                  the arduino'
	@echo  ''
	@echo  '  > Build target options:'
	@echo  '    strict=y    - Turn all warnings into errors'
	@echo  ''
	@echo  'Test targets:'
	@echo  '  tests         - Run all the tests that can run on the host machine'
	@echo  '  bare-tests    - Build all the baremetal tests that run on the target machine'
	@echo  ''
	@echo  'Disk image targets:'
	@echo  '  create-diskimage - Create a new disk image file'
	@echo  '  mount-diskimage  - Mount the disk image file to the default location'
	@echo  '  umount-diskimage - Unmount the disk image file'
	@echo  '  diskimage        - Build `all` and copy the kernel, commands, /etc, and /dev'
	@echo  '                     to the image mountpoint'
	@echo  ''
	@echo  'Global options:'
	@echo  '  O=<dir>       - put all build artifacts and outputs into <dir>'
	@echo  '  C=<file>      - use <file> as the config file for this build'


PHONY += FORCE
FORCE:

.PHONY: $(PHONY)
