
#include <unistd.h>
#include <sys/syscall.h>

pid_t setsid(void)
{
	return SYSCALL1(SYS_SETSID, (int) 0);
}

