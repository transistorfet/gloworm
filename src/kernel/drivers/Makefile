
obj-y				+= block/
#obj-y				+= clock/
obj-$(CONFIG_NET)		+= net/
obj-$(CONFIG_TTY)		+= tty/

diskimage-$(CONFIG_TTY)		+= /dev/tty0-c,2,0
diskimage-$(CONFIG_TTY)		+= /dev/tty1-c,2,1
diskimage-$(CONFIG_MEMDISK)	+= /dev/mem0-c,3,0
diskimage-$(CONFIG_ATA)		+= /dev/ata0-c,4,0

