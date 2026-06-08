
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <kernel/printk.h>
#include <kernel/drivers.h>
#include <kernel/fs/vfs.h>
#include <kernel/utils/iovec.h>

// Address calculations
extern void* __kernel_end;
#if defined(CONFIG_MEM_LAYOUT_AUTO)
#define MEMDISK0_START	__kernel_end
#else
#define MEMDISK0_START	CONFIG_MEMDISK0_START
#endif


// Driver Definition
int mem_init(void);
int mem_open(devminor_t minor, int access);
int mem_close(devminor_t minor);
int mem_read(devminor_t minor, offset_t offset, struct iovec_iter *iter);
int mem_write(devminor_t minor, offset_t offset, struct iovec_iter *iter);
int mem_ioctl(devminor_t minor, unsigned int request, struct iovec_iter *iter, uid_t uid);
int mem_poll(devminor_t minor, int events);
offset_t mem_seek(devminor_t minor, offset_t position, int whence, offset_t offset);


struct driver mem_driver = {
	"mem",
	mem_init,
	mem_open,
	mem_close,
	mem_read,
	mem_write,
	mem_ioctl,
	mem_poll,
	mem_seek,
};

struct mem_geometry {
	char *base;
	size_t size;
};

#define MAX_DEVICES 1
static int num_devices = 0;
static struct mem_geometry devices[MAX_DEVICES];

int mem_add_geometry(char *base, size_t size) {
	if (num_devices >= MAX_DEVICES) {
		log_error("%s: exceeded maximum number of disk devices\n", mem_driver.name);
		return EINVAL;
	}

	devices[num_devices].base = base;
	devices[num_devices].size = size;
	num_devices++;

	return 0;
}

int mem_init(void)
{
	int error;

	num_devices = 0;

	error = mem_add_geometry((char *) MEMDISK0_START, CONFIG_MEMDISK0_SIZE);
	if (error < 0)
		return error;

	error = register_driver(DEVMAJOR_MEM, &mem_driver);
	if (error < 0)
		return error;

	for (short i = 0; i < num_devices; i++)
		log_notice("mem%d: ram disk of %d bytes\n", i, devices[i].size);
	return 0;
}

int mem_open(devminor_t minor, int access)
{
	if (minor >= num_devices)
		return ENXIO;
	return 0;
}

int mem_close(devminor_t minor)
{
	if (minor >= num_devices)
		return ENXIO;
	return 0;
}

int mem_read(devminor_t minor, offset_t offset, struct iovec_iter *iter)
{
	if (minor >= num_devices)
		return ENXIO;
	size_t size = iovec_iter_remaining(iter);
	struct mem_geometry *geo = &devices[minor];

	if (offset > geo->size)
		return EINVAL;
	if (offset + size > geo->size)
		size = geo->size - offset;
	memcpy_into_iter(iter, &geo->base[offset], size);
	return size;
}

int mem_write(devminor_t minor, offset_t offset, struct iovec_iter *iter)
{
	if (minor >= num_devices)
		return ENXIO;
	size_t size = iovec_iter_remaining(iter);
	struct mem_geometry *geo = &devices[minor];

	if (offset > geo->size)
		return EINVAL;
	if (offset + size > geo->size)
		size = geo->size - offset;
	memcpy_out_of_iter(iter, &geo->base[offset], size);
	return size;
}

int mem_ioctl(devminor_t minor, unsigned int request, struct iovec_iter *iter, uid_t uid)
{
	if (minor >= num_devices)
		return ENXIO;
	return EINVAL;
}

int mem_poll(devminor_t minor, int events)
{
	return events & (VFS_POLL_READ | VFS_POLL_WRITE);
}

offset_t mem_seek(devminor_t minor, offset_t position, int whence, offset_t offset)
{
	if (minor >= num_devices)
		return ENXIO;
	struct mem_geometry *geo = &devices[minor];

	switch (whence) {
	    case SEEK_SET:
		break;
	    case SEEK_CUR:
		position = offset + position;
		break;
	    case SEEK_END:
		position = geo->size + position;
		break;
	    default:
		return EINVAL;
	}

	if (position > geo->size)
		position = geo->size;
	return position;

}

