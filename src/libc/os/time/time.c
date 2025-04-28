
#include <sys/types.h>
#include <sys/syscall.h>

time_t time(time_t *t)
{
	return SYSCALL1(SYS_TIME, (int) t);
}

