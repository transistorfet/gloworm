
#ifndef _DIRENT_H
#define _DIRENT_H

#include <limits.h>
#include <sys/types.h>

struct dirent {
	inode_t d_ino;
	char d_name[NAME_MAX];
};

#endif
