
#include <sys/socket.h>
#include <sys/syscall.h>

ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len)
{
	volatile const int opts[2] = { (int) addr, addr_len };
	return SYSCALL5(SYS_SENDTO, fd, (int) buf, n, flags, (int) opts);
}

