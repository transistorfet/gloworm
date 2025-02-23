
always-y		+= libc-$(SRCARCH)-none.a
always-y		+= libc-$(SRCARCH)-os.a

ccflags-$(CONFIG_M68K)	+= -mpcrel


LIBC_NONE_SRCS = $(wildcard $(dir)/arch/$(SRCARCH)/**/*.[cs]) $(wildcard $(dir)/none/**/*.[cs]) $(wildcard $(dir)/common/**/*.[cs])
LIBC_NONE_OBJS = $(addprefix $(OUTPUT),$(patsubst %.c,%.o, $(patsubst %.s,%.o, $(LIBC_NONE_SRCS))))

LIBC_OS_SRCS = $(wildcard $(dir)/arch/$(SRCARCH)/**/*.[cs]) $(wildcard $(dir)/os/**/*.[cs]) $(wildcard $(dir)/common/**/*.[cs])
LIBC_OS_OBJS = $(addprefix $(OUTPUT),$(patsubst %.c,%.o, $(patsubst %.s,%.o, $(LIBC_OS_SRCS))))


#$(info >> $(LIBC_OS_SRCS))
#$(info << $(LIBC_OS_OBJS))

$(objprefix)libc-$(SRCARCH)-none.a: $(LIBC_NONE_OBJS)
$(objprefix)libc-$(SRCARCH)-os.a: $(LIBC_OS_OBJS)
# Only the OS portions of libc depend on the kernel arch includes, in particular syscall.h
$(objprefix)libc-$(SRCARCH)-os.a: CFLAGS += -Isrc/kernel/arch/$(SRCARCH)/include

# Create all the directories, since we're specifying the objects improperly
obj-dirs := $(sort $(patsubst %/,%, $(dir $(LIBC_NONE_OBJS) $(LIBC_OS_OBJS))))
ifneq ($(obj-dirs),)
$(shell mkdir -p $(obj-dirs))
endif

