
#include <sys/syscall.h>

int sync(void)
{
	return SYSCALL1(SYS_SYNC, (int) 0);
}

