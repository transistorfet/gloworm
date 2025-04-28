
#include <sys/types.h>
#include <sys/syscall.h>

int kill(pid_t pid, int sig)
{
	return SYSCALL2(SYS_KILL, (int) pid, (int) sig);
}

