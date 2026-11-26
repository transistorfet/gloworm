
#include <sys/types.h>
#include <sys/syscall.h>

int mknod(const char *path, mode_t mode, dev_t dev)
{
	return SYSCALL3(SYS_MKNOD, (int) path, (int) mode, (int) dev);
}

