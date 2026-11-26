
#include <sys/types.h>
#include <sys/syscall.h>

uid_t getuid(void)
{
	return (uid_t) SYSCALL1(SYS_GETUID, 0);
}

