
#include <unistd.h>
#include <sys/syscall.h>

off_t lseek(int fd, off_t offset, int whence)
{
	return SYSCALL3(SYS_LSEEK, fd, (int) offset, (int) whence);
}

