

#include <sys/socket.h>
#include <sys/syscall.h>

int listen(int fd, int n)
{
	return SYSCALL2(SYS_LISTEN, fd, (int) n);
}

