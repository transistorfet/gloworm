
#include <sys/types.h>
#include <sys/syscall.h>

int umount(const char *source)
{
	return SYSCALL1(SYS_UMOUNT, (int) source);
}

