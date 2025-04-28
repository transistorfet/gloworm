
#include <unistd.h>
#include <sys/syscall.h>

offset_t lseek(int fd, offset_t offset, int whence)
{
	return SYSCALL3(SYS_LSEEK, fd, (int) offset, (int) whence);
}

