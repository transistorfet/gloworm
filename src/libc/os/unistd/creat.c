
#include <unistd.h>
#include <sys/syscall.h>

int creat(const char *path, mode_t mode)
{
	return SYSCALL2(SYS_CREAT, (int) path, (int) mode);
}

