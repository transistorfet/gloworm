
always-y	+= libc-68k-none.a
always-y	+= libc-68k-os.a

ccflags-y += -mpcrel


LIBC_68K_NONE_SRCS = $(wildcard $(dir)/68k/**/*.[cs]) $(wildcard $(dir)/none/**/*.[cs]) $(wildcard $(dir)/common/**/*.[cs])
LIBC_68K_NONE_OBJS = $(addprefix $(OUTPUT),$(patsubst %.c,%.o, $(patsubst %.s,%.o, $(LIBC_68K_NONE_SRCS))))

LIBC_68K_OS_SRCS = $(wildcard $(dir)/68k/**/*.[cs]) $(wildcard $(dir)/os/**/*.[cs]) $(wildcard $(dir)/common/**/*.[cs])
LIBC_68K_OS_OBJS = $(addprefix $(OUTPUT),$(patsubst %.c,%.o, $(patsubst %.s,%.o, $(LIBC_68K_OS_SRCS))))

#$(info >> $(LIBC_68K_OS_SRCS))
#$(info << $(LIBC_68K_OS_OBJS))

$(objprefix)libc-68k-none.a: $(LIBC_68K_NONE_OBJS)
$(objprefix)libc-68k-os.a: $(LIBC_68K_OS_OBJS)

# Create all the directories, since we're specifying the objects improperly
obj-dirs := $(sort $(patsubst %/,%, $(dir $(LIBC_68K_NONE_OBJS) $(LIBC_68K_OS_OBJS))))
ifneq ($(obj-dirs),)
$(shell mkdir -p $(obj-dirs))
endif

