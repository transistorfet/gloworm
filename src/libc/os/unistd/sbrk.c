
#include <unistd.h>
#include <sys/syscall.h>

void *sbrk(intptr_t increment)
{
	return (void *) SYSCALL1(SYS_SBRK, (int) increment);
}

