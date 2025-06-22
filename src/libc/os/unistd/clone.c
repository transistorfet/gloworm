
#include <unistd.h>
#include <sys/syscall.h>

pid_t clone(int (*fn)(void *), void *stack, int flags, void *arg)
{
	return SYSCALL5(SYS_CLONE, (unsigned long) fn, (unsigned long) stack, flags, (unsigned long) arg, 0);
}

