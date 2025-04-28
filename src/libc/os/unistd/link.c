
#include <sys/types.h>
#include <sys/syscall.h>

int link(const char *oldpath, const char *newpath)
{
	return SYSCALL2(SYS_LINK, (int) oldpath, (int) newpath);
}

