
#include <sys/syscall.h>

int pause(void)
{
	return SYSCALL1(SYS_PAUSE, (int) 0);
}

