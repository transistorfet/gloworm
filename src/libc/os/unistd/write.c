
#include <unistd.h>
#include <kernel/syscall.h>


ssize_t write(int fd, const void *buf, size_t nbytes)
{
	return SYSCALL3(SYS_WRITE, fd, (int) buf, (int) nbytes);
}
