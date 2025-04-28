
#include <unistd.h>
#include <sys/syscall.h>

pid_t fork()
{
	return SYSCALL1(SYS_FORK, 0);
}

