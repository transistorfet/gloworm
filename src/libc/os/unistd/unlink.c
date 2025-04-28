
#include <unistd.h>
#include <sys/syscall.h>

int unlink(const char *path)
{
	return SYSCALL2(SYS_UNLINK, (int) path, 0);
}

