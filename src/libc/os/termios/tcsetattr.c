
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <attributes.h>

int tcsetattr(int fd, __unused int optional_actions, const struct termios *tio)
{
	return ioctl(fd, TCSETS, (void *) tio);
}
