
#include <sys/types.h>
#include <sys/syscall.h>

int setpgid(pid_t pid, pid_t pgid)
{
	return SYSCALL2(SYS_SETPGID, (int) pid, (int) pgid);
}

