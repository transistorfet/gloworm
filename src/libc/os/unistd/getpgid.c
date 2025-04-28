
#include <sys/types.h>
#include <sys/syscall.h>

pid_t getpgid(pid_t pid)
{
	return SYSCALL1(SYS_GETPGID, (int) pid);
}

