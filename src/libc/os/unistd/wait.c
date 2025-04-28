
#include <unistd.h>
#include <sys/syscall.h>

pid_t wait(int *status)
{
	return SYSCALL3(SYS_WAITPID, -1, (int) status, 0);
}

