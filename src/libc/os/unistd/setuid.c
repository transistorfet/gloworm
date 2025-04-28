
#include <unistd.h>
#include <sys/syscall.h>

int setuid(uid_t uid)
{
	return SYSCALL1(SYS_SETUID, (int) uid);
}

