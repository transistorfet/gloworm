
#ifndef _NONE_STDIO_FILE_H
#define _NONE_STDIO_FILE_H

#define _STDIO_BASE_FD		3
#define _STDIO_MAX_OPEN_FILES	FOPEN_MAX

struct _FILE {
	int fd;
};

#endif

