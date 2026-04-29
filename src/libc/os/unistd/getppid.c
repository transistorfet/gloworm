
#include <sys/types.h>
#include <sys/syscall.h>

pid_t getppid(void)
{
	return SYSCALL1(SYS_GETPPID, 0);
}

