
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <kernel/printk.h>
#include <kernel/drivers.h>
#include <kernel/fs/vfs.h>


// Driver Definition
int mem_init();
int mem_open(devminor_t minor, int access);
int mem_close(devminor_t minor);
int mem_read(devminor_t minor, char *buffer, offset_t offset, size_t size);
int mem_write(devminor_t minor, const char *buffer, offset_t offset, size_t size);
int mem_ioctl(devminor_t minor, unsigned int request, void *argp, uid_t uid);
int mem_poll(devminor_t minor, int events);
offset_t mem_seek(devminor_t minor, offset_t position, int whence, offset_t offset);


struct driver mem_driver = {
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

static int num_devices = 1;
static struct mem_geometry devices[] = {
	{ (char *) 0x1E0000, 0x20000 },
};


int mem_init()
{
	register_driver(DEVMAJOR_MEM, &mem_driver);

	for (short i = 0; i < num_devices; i++)
		printk_safe("mem%d: ram disk of %d bytes\n", i, devices[i].size);
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

int mem_read(devminor_t minor, char *buffer, offset_t offset, size_t size)
{
	if (minor >= num_devices)
		return ENXIO;
	struct mem_geometry *geo = &devices[minor];

	if (offset > geo->size)
		return -1;
	if (offset + size > geo->size)
		size = geo->size - offset;
	memcpy(buffer, &geo->base[offset], size);
	return size;
}

int mem_write(devminor_t minor, const char *buffer, offset_t offset, size_t size)
{
	if (minor >= num_devices)
		return ENXIO;
	struct mem_geometry *geo = &devices[minor];

	if (offset > geo->size)
		return -1;
	if (offset + size > geo->size)
		size = geo->size - offset;
	memcpy(&geo->base[offset], buffer, size);
	return size;
}

int mem_ioctl(devminor_t minor, unsigned int request, void *argp, uid_t uid)
{
	if (minor >= num_devices)
		return ENXIO;
	return -1;
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
