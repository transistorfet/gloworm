
#include <unistd.h>
#include <sys/syscall.h>

int dup2(int oldfd, int newfd)
{
	return SYSCALL2(SYS_DUP2, (int) oldfd, (int) newfd);
}

