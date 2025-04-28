
#include <unistd.h>
#include <sys/syscall.h>

int stat(const char *path, struct stat *statbuf)
{
	return SYSCALL2(SYS_STAT, (int) path, (int) statbuf);
}

