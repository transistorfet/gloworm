
#include <unistd.h>
#include <sys/syscall.h>

mode_t umask(mode_t mask)
{
	return (mode_t) SYSCALL1(SYS_UMASK, (int) mask);
}

