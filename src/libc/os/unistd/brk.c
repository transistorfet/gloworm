
#include <unistd.h>
#include <sys/syscall.h>

int brk(void *addr)
{
	return SYSCALL1(SYS_BRK, (int) addr);
}

