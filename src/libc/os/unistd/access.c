
#include <unistd.h>
#include <sys/syscall.h>

int access(const char *path, int mode)
{
	return SYSCALL2(SYS_ACCESS, (int) path, (int) mode);
}

