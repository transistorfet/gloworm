
#include <sys/types.h>
#include <sys/syscall.h>

pid_t getpid(void)
{
	return SYSCALL1(SYS_GETPID, 0);
}

