
#include <unistd.h>
#include <sys/syscall.h>

pid_t fork(void)
{
	return SYSCALL1(SYS_FORK, 0);
}

