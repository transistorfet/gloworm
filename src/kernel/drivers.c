
#include <errno.h>
#include <sys/types.h>
#include <kernel/drivers.h>
#include <kernel/utils/iovec.h>

#define MAX_DRIVERS	6

static struct driver *drv_table[MAX_DRIVERS];

int register_driver(devmajor_t major, struct driver *driver)
{
	if (major >= MAX_DRIVERS) {
		return -1;
	}

	if (drv_table[major]) {
		log_error("register_driver: a driver is already registered under the major number %d\n", major);
		return -1;
	}

	if (!driver->init || !driver->open || !driver->close || !driver->read || !driver->write || !driver->ioctl || !driver->poll || !driver->seek) {
		log_error("register_driver: missing ops function for driver major %d\n", major);
		return -1;
	}

	drv_table[major] = driver;
	return 0;
}

int dev_open(device_t dev, int access)
{
	devmajor_t major = dev >> 8;
	devminor_t minor = (devminor_t) dev;

	if (major >= MAX_DRIVERS)
		return ENXIO;
	return drv_table[major]->open(minor, access);
}

int dev_close(device_t dev)
{
	devmajor_t major = dev >> 8;
	devminor_t minor = (devminor_t) dev;

	if (major >= MAX_DRIVERS)
		return ENXIO;
	return drv_table[major]->close(minor);
}

int dev_read(device_t dev, offset_t offset, struct iovec_iter *iter)
{
	devmajor_t major = dev >> 8;
	devminor_t minor = (devminor_t) dev;

	if (major >= MAX_DRIVERS)
		return ENXIO;
	return drv_table[major]->read(minor, offset, iter);
}

int dev_write(device_t dev, offset_t offset, struct iovec_iter *iter)
{
	devmajor_t major = dev >> 8;
	devminor_t minor = (devminor_t) dev;

	if (major >= MAX_DRIVERS)
		return ENXIO;
	return drv_table[major]->write(minor, offset, iter);
}

int dev_ioctl(device_t dev, unsigned int request, struct iovec_iter *iter, uid_t uid)
{
	devmajor_t major = dev >> 8;
	devminor_t minor = (devminor_t) dev;

	if (major >= MAX_DRIVERS)
		return ENXIO;
	return drv_table[major]->ioctl(minor, request, iter, uid);
}

int dev_poll(device_t dev, int events)
{
	devmajor_t major = dev >> 8;
	devminor_t minor = (devminor_t) dev;

	if (major >= MAX_DRIVERS)
		return ENXIO;
	return drv_table[major]->poll(minor, events);
}

offset_t dev_seek(device_t dev, offset_t position, int whence, offset_t offset)
{
	devmajor_t major = dev >> 8;
	devminor_t minor = (devminor_t) dev;

	if (major >= MAX_DRIVERS)
		return ENXIO;
	return drv_table[major]->seek(minor, position, whence, offset);
}

