
#include <unistd.h>
#include <sys/syscall.h>

int fstat(int fd, struct stat *statbuf)
{
	return SYSCALL2(SYS_FSTAT, (int) fd, (int) statbuf);
}

