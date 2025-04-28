
#include <unistd.h>
#include <sys/syscall.h>

int chdir(const char *path)
{
	return SYSCALL1(SYS_CHDIR, (int) path);
}

