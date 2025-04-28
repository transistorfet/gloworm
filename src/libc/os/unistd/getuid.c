
#include <sys/types.h>
#include <sys/syscall.h>

uid_t getuid()
{
	return SYSCALL1(SYS_GETUID, 0);
}

