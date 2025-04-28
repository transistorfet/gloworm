
#include <sys/syscall.h>

struct dirent;

int readdir(int fd, struct dirent *dir)
{
	return SYSCALL3(SYS_READDIR, fd, (int) dir, 0);
}

