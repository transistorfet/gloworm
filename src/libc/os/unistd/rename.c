
#include <unistd.h>
#include <sys/syscall.h>

int rename(const char *oldpath, const char *newpath)
{
	return SYSCALL2(SYS_RENAME, (int) oldpath, (int) newpath);
}

