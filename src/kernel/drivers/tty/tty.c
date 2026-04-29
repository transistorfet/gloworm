
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioc_tty.h>

#include <kernel/drivers.h>
#include <kernel/syscall.h>
#include <kernel/irq/bh.h>
#include <kernel/fs/vfs.h>
#include <kernel/proc/signal.h>
#include <kernel/proc/scheduler.h>
#include <kernel/utils/iovec.h>

#include <asm/irqs.h>


#define TTY_DEVICE_NUM		2

#define MAX_CANNON		256

struct tty_device {
	device_t rdev;
	pid_t pgid;

	struct termios tio;

	int error;
	char ready;
	char escape;
	short opens;
	short buf_read;
	short buf_write;
	char buffer[MAX_CANNON];
};

struct termios default_tio = {
	.c_iflag = (BRKINT | ICRNL | IXON | IXANY),
	.c_oflag = (OPOST | ONLCR),
	.c_cflag = (CREAD | CS8 | HUPCL),
	.c_lflag = (ISIG | IEXTEN | ICANON | ECHO | ECHOE),
	.c_cc = {
		'\x04',		// VEOF
		0xff,		// VEOL
		'\x08',		// VERASE
		'\x03',		// VINTR
		'\x15',		// VKILL
		1,		// VMIN
		'\x1C',		// VQUIT
		0,		// VTIME
		'\x1A',		// VSUSP
		'\x13',		// VSTART
		'\x11',		// VSTOP
	}
};


// Driver Definition
int tty_init(void);
int tty_open(devminor_t minor, int access);
int tty_close(devminor_t minor);
int tty_read(devminor_t minor, offset_t offset, struct iovec_iter *iter);
int tty_write(devminor_t minor, offset_t offset, struct iovec_iter *iter);
int tty_ioctl(devminor_t minor, unsigned int request, struct iovec_iter *iter, uid_t uid);
int tty_poll(devminor_t minor, int events);

struct driver tty_driver = {
	tty_init,
	tty_open,
	tty_close,
	tty_read,
	tty_write,
	tty_ioctl,
	tty_poll,
	NULL,
};


static struct tty_device tty_devices[TTY_DEVICE_NUM];

static void tty_reset_input(device_t minor)
{
	tty_devices[minor].ready = 0;
	tty_devices[minor].buf_read = 0;
	tty_devices[minor].buf_write = 0;
}


static inline void tty_read_input(device_t minor)
{
	char ch;
	int error;
	struct kvec kvec;
	struct iovec_iter iter;
	struct tty_device *tty = &tty_devices[minor];

	while (1) {
		// If the buffer is full, then consider the line to be terminated and resume the waiting proc
		if (tty->buf_write >= MAX_CANNON) {
			resume_blocked_procs(VFS_POLL_READ, NULL, DEVNUM(DEVMAJOR_TTY, minor));
			tty->ready = 1;
			return;
		}

		iovec_iter_init_simple_kvec(&iter, &kvec, &ch, 1);
		error = dev_read(tty->rdev, 0, &iter);
		if (error <= 0) {
			tty->error = error;
			return;
		}

		if ((tty->tio.c_lflag & ISIG)) {
			if (ch == tty->tio.c_cc[VINTR]) {
				tty_reset_input(minor);
				send_signal_process_group(tty->pgid, SIGINT);
				return;
			} else if (ch == tty->tio.c_cc[VSUSP]) {
				send_signal_process_group(tty->pgid, SIGSTOP);
				return;
			}
		}


		if (ch == '\x1B') {
			tty->escape = 1;
		} else if (tty->escape == 1) {
			tty->escape = (ch == '[') ? 2 : 0;
		} else if (tty->escape == 2) {
			// TODO intepret the next character (or buffer somewhere if not a letter
			tty->escape = 0;
		} else if (ch == tty->tio.c_cc[VERASE]) {
			if (tty->buf_write >= 1) {
				tty->buf_write--;
				if ((tty->tio.c_lflag & ECHOE)) {
					if (tty->buffer[tty->buf_write] == '\t') {
						// TODO proper handling of this might require window dimensions and cursor position tracking
						iovec_iter_init_simple_kvec(&iter, &kvec, "\x08\x08\x08\x08\x08\x08\x08\x08", 8);
						dev_write(tty->rdev, 0, &iter);
					} else {
						iovec_iter_init_simple_kvec(&iter, &kvec, "\x08 \x08", 3);
						dev_write(tty->rdev, 0, &iter);
					}
				}
			}
		} else if (ch == '\r') {
			// Ignore
		} else {
			tty->buffer[tty->buf_write++] = ch;
			if ((tty->tio.c_lflag & ECHO)) {
				iovec_iter_init_simple_kvec(&iter, &kvec, &ch, 1);
				dev_write(tty->rdev, 0, &iter);
			}
			if (ch == '\n') {
				tty->ready = 1;
				resume_blocked_procs(VFS_POLL_READ, NULL, DEVNUM(DEVMAJOR_TTY, minor));
				break;
			}
		}
	}
}

static void tty_process_input(void *_unused)
{
	for (short minor = 0; minor < TTY_DEVICE_NUM; minor++) {
		if (tty_devices[minor].opens && (tty_devices[minor].tio.c_lflag & ICANON)) {
			tty_read_input(minor);
		}
	}
}

int tty_init(void)
{
	for (short i = 0; i < TTY_DEVICE_NUM; i++) {
		tty_devices[i].rdev = DEVNUM(DEVMAJOR_TTY68681, i);
		tty_devices[i].pgid = 0;
		tty_devices[i].error = 0;
		tty_devices[i].opens = 0;
		tty_devices[i].ready = 0;
		tty_devices[i].buf_read = 0;
		tty_devices[i].buf_write = 0;

		memcpy(&tty_devices[i].tio, &default_tio, sizeof(struct termios));
	}

	register_driver(DEVMAJOR_TTY, &tty_driver);
	register_bh(BH_TTY, tty_process_input, NULL);
	enable_bh(BH_TTY);
	return 0;
}

int tty_open(devminor_t minor, int mode)
{
	if (minor >= TTY_DEVICE_NUM)
		return ENODEV;
	// TODO you need to notify the serial driver to request the tty bottom half run after any input
	if (tty_devices[minor].opens++ == 0)
		return dev_open(tty_devices[minor].rdev, mode | O_NONBLOCK | O_EXCL);
	return 0;
}

int tty_close(devminor_t minor)
{
	if (minor >= TTY_DEVICE_NUM)
		return ENODEV;
	if (tty_devices[minor].opens == 0)
		return EBADF;
	if (--tty_devices[minor].opens == 0)
		return dev_close(tty_devices[minor].rdev);
	return 0;
}

int tty_read(devminor_t minor, offset_t offset, struct iovec_iter *iter)
{
	size_t size;

	if (minor >= TTY_DEVICE_NUM)
		return ENODEV;

	// If an error occurred when buffering the data, then return it and clear the error
	if (tty_devices[minor].error) {
		int error = tty_devices[minor].error;
		tty_devices[minor].error = 0;
		return error;
	}

	size = iovec_iter_remaining(iter);
	if (!(tty_devices[minor].tio.c_lflag & ICANON)) {
		if (tty_devices[minor].buf_read < tty_devices[minor].buf_write) {
			size_t max = tty_devices[minor].buf_write - tty_devices[minor].buf_read;
			if (size < max)
				max = size;
			memcpy_into_iter(iter, &tty_devices[minor].buffer[tty_devices[minor].buf_read], max);
			tty_devices[minor].buf_read += max;
			if (tty_devices[minor].buf_read >= tty_devices[minor].buf_write)
				tty_reset_input(minor);
			return max;
		}

		int read = dev_read(tty_devices[minor].rdev, offset, iter);
		if (read == 0) {
			suspend_current_syscall(VFS_POLL_READ);
			return 0;
		}
		return read;
	} else {
		size_t max;
		size_t count;
		uint8_t byte;

		// If an entire line is not available yet, then suspend the process
		if (!tty_devices[minor].ready) {
			suspend_current_syscall(VFS_POLL_READ);
			return 0;
		}

		// Copy the lesser of the remaining buffered bytes or the requested size
		max = tty_devices[minor].buf_write - tty_devices[minor].buf_read;
		if (size < max)
			max = size;
		for (count = 0; count < max; count++) {
			byte = tty_devices[minor].buffer[tty_devices[minor].buf_read++];
			copy_uint8_into_iter(iter, byte);

			// If we reach the end of a line, shift any remaining data over to make room, and exit the loop
			if (byte == '\n') {
				count += 1;
				int remaining = tty_devices[minor].buf_write - tty_devices[minor].buf_read;
				if (remaining) {
					memcpy(tty_devices[minor].buffer, &tty_devices[minor].buffer[tty_devices[minor].buf_read], remaining);
					tty_devices[minor].buf_write -= tty_devices[minor].buf_read;
					tty_devices[minor].buf_read = 0;
				}
				break;
			}
		}

		// If an entire line has been read, then reset the buffer and attempt to read more input from the raw device
		if (tty_devices[minor].buf_read >= tty_devices[minor].buf_write) {
			tty_reset_input(minor);
			request_bh_run(BH_TTY);
		}
		return count;
	}
}

int tty_write(devminor_t minor, offset_t offset, struct iovec_iter *iter)
{
	int written;

	if (minor >= TTY_DEVICE_NUM)
		return ENODEV;

	written = dev_write(tty_devices[minor].rdev, offset, iter);
	if (!written) {
		suspend_current_syscall(VFS_POLL_WRITE);
		return 0;
	}
	return written;
}

int tty_ioctl(devminor_t minor, unsigned int request, struct iovec_iter *iter, uid_t uid)
{
	short saved_status;

	if (minor >= TTY_DEVICE_NUM)
		return ENODEV;

	switch (request) {
		case TCGETS: {
			LOCK(saved_status);
			memcpy_into_iter(iter, &tty_devices[minor].tio, sizeof(struct termios));
			UNLOCK(saved_status);
			return 0;
		}
		case TCSETS: {
			LOCK(saved_status);
			memcpy_out_of_iter(iter, &tty_devices[minor].tio, sizeof(struct termios));
			UNLOCK(saved_status);
			return 0;
		}

		case TIOCGPGRP: {
			int arg;
			arg = tty_devices[minor].pgid;
			memcpy_into_iter(iter, &arg, sizeof(int));
			return 0;
		}
		case TIOCSPGRP: {
			int arg;
			memcpy_out_of_iter(iter, &arg, sizeof(int));
			tty_devices[minor].pgid = arg;
			return 0;
		}
		default:
			return dev_ioctl(tty_devices[minor].rdev, request, iter, uid);
	}
}

int tty_poll(devminor_t minor, int events)
{
	int revents = 0;
	struct tty_device *tty = &tty_devices[minor];

	if ((events & VFS_POLL_READ) && tty->opens > 0 && tty->ready)
		revents |= VFS_POLL_READ;
	if ((events & VFS_POLL_WRITE) && tty->opens > 0)
		revents |= dev_poll(tty_devices[minor].rdev, VFS_POLL_WRITE);
	if ((events & VFS_POLL_ERROR) && tty->opens > 0 && tty->error)
		revents |= VFS_POLL_ERROR;

	return revents;
}

