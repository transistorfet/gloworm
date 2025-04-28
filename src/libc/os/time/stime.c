
#include <sys/types.h>
#include <sys/syscall.h>

int stime(const time_t *t)
{
	return SYSCALL1(SYS_STIME, (int) t);
}

