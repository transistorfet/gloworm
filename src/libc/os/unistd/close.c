
#include <unistd.h>
#include <sys/syscall.h>

int close(int fd)
{
	return SYSCALL1(SYS_CLOSE, fd);
}

