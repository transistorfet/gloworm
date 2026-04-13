
#ifndef _SRC_KERNEL_FS_PROCFS_DATA_H
#define _SRC_KERNEL_FS_PROCFS_DATA_H

#include <kconfig.h>

struct process;

int get_data_cmdline(struct process *proc, char *buffer, int max);
int get_data_stat(struct process *proc, char *buffer, int max);
int get_data_statm(struct process *proc, char *buffer, int max);
int get_data_maps(struct process *proc, char *buffer, int max);
#if defined(CONFIG_MMU)
int get_data_pagedump(struct process *proc, char *buffer, int max);
#endif

int get_data_mounts(struct process *proc, char *buffer, int max);

#endif
