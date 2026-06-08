
#include <errno.h>
#include <sys/types.h>
#include <kernel/drivers.h>
#include <kernel/utils/iovec.h>

#define MAX_DRIVERS	6

static struct driver *drv_table[MAX_DRIVERS];

void init_drivers(void)
{
	for (int i = 0; i < MAX_DRIVERS; i++) {
		drv_table[i] = NULL;
	}
}

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

struct driver *get_driver(devmajor_t major)
{
	if (major >= MAX_DRIVERS)
		return NULL;
	return drv_table[major];
}

int dev_open(device_t dev, int access)
{
	const devmajor_t major = DEVMAJOR(dev);
	const devminor_t minor = DEVMINOR(dev);

	if (major >= MAX_DRIVERS || !drv_table[major])
		return ENXIO;
	return drv_table[major]->open(minor, access);
}

int dev_close(device_t dev)
{
	const devmajor_t major = DEVMAJOR(dev);
	const devminor_t minor = DEVMINOR(dev);

	if (major >= MAX_DRIVERS || !drv_table[major])
		return ENXIO;
	return drv_table[major]->close(minor);
}

int dev_read(device_t dev, offset_t offset, struct iovec_iter *iter)
{
	const devmajor_t major = DEVMAJOR(dev);
	const devminor_t minor = DEVMINOR(dev);

	if (major >= MAX_DRIVERS || !drv_table[major])
		return ENXIO;
	return drv_table[major]->read(minor, offset, iter);
}

int dev_write(device_t dev, offset_t offset, struct iovec_iter *iter)
{
	const devmajor_t major = DEVMAJOR(dev);
	const devminor_t minor = DEVMINOR(dev);

	if (major >= MAX_DRIVERS || !drv_table[major])
		return ENXIO;
	return drv_table[major]->write(minor, offset, iter);
}

int dev_ioctl(device_t dev, unsigned int request, struct iovec_iter *iter, uid_t uid)
{
	const devmajor_t major = DEVMAJOR(dev);
	const devminor_t minor = DEVMINOR(dev);

	if (major >= MAX_DRIVERS || !drv_table[major])
		return ENXIO;
	return drv_table[major]->ioctl(minor, request, iter, uid);
}

int dev_poll(device_t dev, int events)
{
	const devmajor_t major = DEVMAJOR(dev);
	const devminor_t minor = DEVMINOR(dev);

	if (major >= MAX_DRIVERS || !drv_table[major])
		return ENXIO;
	return drv_table[major]->poll(minor, events);
}

offset_t dev_seek(device_t dev, offset_t position, int whence, offset_t offset)
{
	const devmajor_t major = DEVMAJOR(dev);
	const devminor_t minor = DEVMINOR(dev);

	if (major >= MAX_DRIVERS || !drv_table[major])
		return ENXIO;
	return drv_table[major]->seek(minor, position, whence, offset);
}

