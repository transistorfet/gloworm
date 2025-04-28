
#include <unistd.h>
#include <sys/syscall.h>

mode_t umask(mode_t mask)
{
	return SYSCALL1(SYS_UMASK, (int) mask);
}

