
#include <sys/socket.h>
#include <sys/syscall.h>

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	return SYSCALL3(SYS_CONNECT, fd, (int) addr, (int) len);
}

