
#include <unistd.h>
#include <kernel/syscall.h>


ssize_t read(int fd, void *buf, size_t nbytes)
{
	return SYSCALL3(SYS_READ, fd, (int) buf, (int) nbytes);
}
