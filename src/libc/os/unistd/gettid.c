
#include <sys/types.h>
#include <sys/syscall.h>

pid_t gettid()
{
	return SYSCALL1(SYS_GETTID, 0);
}

