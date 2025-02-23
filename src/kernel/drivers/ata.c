
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <kernel/vfs.h>
#include <kernel/printk.h>
#include <kernel/driver.h>
#include <kernel/kconfig.h>

#include "partition.h"
#include <asm/interrupts.h>


// Driver Definition
int ata_init();
int ata_open(devminor_t minor, int access);
int ata_close(devminor_t minor);
int ata_read(devminor_t minor, char *buffer, offset_t offset, size_t size);
int ata_write(devminor_t minor, const char *buffer, offset_t offset, size_t size);
int ata_ioctl(devminor_t minor, unsigned int request, void *argp, uid_t uid);
int ata_poll(devminor_t minor, int events);
offset_t ata_seek(devminor_t minor, offset_t position, int whence, offset_t offset);


struct driver ata_driver = {
	ata_init,
	ata_open,
	ata_close,
	ata_read,
	ata_write,
	ata_ioctl,
	ata_poll,
	ata_seek,
};


#define COMET_VME_CF_CONTROL	((volatile uint8_t *) 0xff800020)
#define ATA_REG_BASE		CONFIG_ATA_BASE
#define ATA_REG_DEV_CONTROL	((volatile uint8_t *) (ATA_REG_BASE + 0x1c))
#define ATA_REG_DEV_ADDRESS	((volatile uint8_t *) (ATA_REG_BASE + 0x1e))

#define ATA_REG_DATA		((volatile uint16_t *) (ATA_REG_BASE + 0x0))
#define ATA_REG_DATA_BYTE	((volatile uint8_t *) (ATA_REG_BASE + 0x0))
#define ATA_REG_FEATURE		((volatile uint8_t *) (ATA_REG_BASE + 0x2))
#define ATA_REG_ERROR		((volatile uint8_t *) (ATA_REG_BASE + 0x2))
#define ATA_REG_SECTOR_COUNT	((volatile uint8_t *) (ATA_REG_BASE + 0x4))
#define ATA_REG_SECTOR_NUM	((volatile uint8_t *) (ATA_REG_BASE + 0x6))
#define ATA_REG_CYL_LOW		((volatile uint8_t *) (ATA_REG_BASE + 0x8))
#define ATA_REG_CYL_HIGH	((volatile uint8_t *) (ATA_REG_BASE + 0xa))
#define ATA_REG_DRIVE_HEAD	((volatile uint8_t *) (ATA_REG_BASE + 0xc))
#define ATA_REG_STATUS		((volatile uint8_t *) (ATA_REG_BASE + 0xe))
#define ATA_REG_COMMAND		((volatile uint8_t *) (ATA_REG_BASE + 0xe))


#define ATA_CMD_READ_SECTORS	0x20
#define ATA_CMD_WRITE_SECTORS	0x30
#define ATA_CMD_IDENTIFY	0xEC
#define ATA_CMD_SET_FEATURE	0xEF

#define ATA_ST_BUSY		0x80
#define ATA_ST_DATA_READY	0x08
#define ATA_ST_ERROR		0x01


#define ATA_DELAY(x)		{ for (int delay = 0; delay < (x); delay++) { asm volatile(""); } }
#define ATA_WAIT()		{ ATA_DELAY(8); while (*ATA_REG_STATUS & ATA_ST_BUSY) { } }
#define ATA_WAIT_FOR_DATA()	{ while (!((*ATA_REG_STATUS) & ATA_ST_DATA_READY)) { ATA_DELAY(8); } }

/*
static inline void ATA_DELAY(short delay)
{
	for (; delay > 0; delay--) { asm volatile(""); }
}

static inline void ATA_WAIT()
{
	ATA_DELAY(4);
	while (*ATA_REG_STATUS & ATA_ST_BUSY) { }
}
*/

int ata_detect()
{
	uint8_t status;

	status = *ATA_REG_STATUS;
	// If the busy bit is already set, or the two bits that are always 0, then perhaps nothing is connected
	if (status & (ATA_ST_BUSY | 0x06))
		return 0;
	ATA_DELAY(10);

	// Reset the IDE bus
	(*ATA_REG_COMMAND) = ATA_CMD_IDENTIFY;

	for (int i = 0; i < 1000; i++) {
		ATA_DELAY(10);

		status = *ATA_REG_STATUS;
		// If it becomes unbusy within the timeout then a drive is connected
		if (!(status & ATA_ST_BUSY)) {
			if (status & ATA_ST_DATA_READY) {
				ATA_DELAY(100);
				return 1;
			} else {
				return 0;
			}
		}
	}
	return 0;
}

/*
int ata_reset()
{
	(*ATA_REG_DEV_CONTROL) = 0x04;
	ATA_DELAY(16);
	(*ATA_REG_DEV_CONTROL) = 0x00;
	ATA_DELAY(16);
	while (*ATA_REG_STATUS & ATA_ST_BUSY) { }
	return 0;
}
*/

int ata_read_sector(int sector, char *buffer)
{
	short saved_status;

	LOCK(saved_status);

	// Set 8-bit mode
	(*ATA_REG_FEATURE) = 0x01;
	(*ATA_REG_COMMAND) = ATA_CMD_SET_FEATURE;
	ATA_WAIT();

	// Read a sector
	(*ATA_REG_DRIVE_HEAD) = 0xE0;
	//(*ATA_REG_DRIVE_HEAD) = 0xE0 | (uint8_t) ((sector >> 24) & 0x0F);
	(*ATA_REG_CYL_HIGH) = (uint8_t) (sector >> 16);
	(*ATA_REG_CYL_LOW) = (uint8_t) (sector >> 8);
	(*ATA_REG_SECTOR_NUM) = (uint8_t) sector;
	(*ATA_REG_SECTOR_COUNT) = 1;
	(*ATA_REG_COMMAND) = ATA_CMD_READ_SECTORS;
	ATA_WAIT();

	char status = (*ATA_REG_STATUS);
	if (status & 0x01) {
		printk_safe("Error while reading ata: %x\n", (*ATA_REG_ERROR));
		UNLOCK(saved_status);
		return 0;
	}

	ATA_WAIT();
	ATA_WAIT_FOR_DATA();

	for (int i = 0; i < 512; i++) {
		//((uint16_t *) buffer)[i] = (*ATA_REG_DATA);
		//asm volatile("rol.w	#8, %0\n" : "+g" (((uint16_t *) buffer)[i]));
		buffer[i] = (*ATA_REG_DATA_BYTE);

		ATA_WAIT();
		//ATA_DELAY(10);
	}

	/*
	printk_safe("reading sector %x:\n", sector);
	for (int i = 0; i < 512; i++) {
		printk_safe("%02x ", 0xff & buffer[i]);
		if ((i & 0x1F) == 0x1F)
			printk_safe("\n");
	}
	*/

	UNLOCK(saved_status);
	return 512;
}

int ata_write_sector(int sector, const char *buffer)
{
	short saved_status;

	LOCK(saved_status);

	// Set 8-bit mode
	(*ATA_REG_FEATURE) = 0x01;
	(*ATA_REG_COMMAND) = ATA_CMD_SET_FEATURE;
	ATA_WAIT();

	// Read a sector
	(*ATA_REG_DRIVE_HEAD) = 0xE0;
	//(*ATA_REG_DRIVE_HEAD) = 0xE0 | (uint8_t) ((sector >> 24) & 0x0F);
	(*ATA_REG_CYL_HIGH) = (uint8_t) (sector >> 16);
	(*ATA_REG_CYL_LOW) = (uint8_t) (sector >> 8);
	(*ATA_REG_SECTOR_NUM) = (uint8_t) sector;
	(*ATA_REG_SECTOR_COUNT) = 1;
	(*ATA_REG_COMMAND) = ATA_CMD_WRITE_SECTORS;
	ATA_WAIT();

	char status = (*ATA_REG_STATUS);
	//printk_safe("IDE: %x\n", status);
	if (status & 0x01) {
		printk_safe("Error while writing ata: %x\n", (*ATA_REG_ERROR));
		UNLOCK(saved_status);
		return 0;
	}

	ATA_DELAY(100);
	ATA_WAIT();
	ATA_WAIT_FOR_DATA();

	for (int i = 0; i < 512; i++) {
		ATA_WAIT();
		//while (((*ATA_REG_STATUS) & ATA_ST_BUSY) || !((*ATA_REG_STATUS) & ATA_ST_DATA_READY)) { }

		//((uint16_t *) buffer)[i] = (*ATA_REG_DATA);
		//asm volatile("rol.w	#8, %0\n" : "+g" (((uint16_t *) buffer)[i]));
		(*ATA_REG_DATA_BYTE) = buffer[i];
	}

	ATA_WAIT();

	if (*ATA_REG_STATUS & ATA_ST_ERROR) {
		printk_safe("Error writing sector %d: %x\n", sector, *ATA_REG_ERROR);
	}

	UNLOCK(saved_status);

	return 512;
}


// Driver-specific data
struct ata_drive {
	struct partition parts[PARTITION_MAX];
};

#define ATA_MAX_DRIVES		1

static struct ata_drive drives[ATA_MAX_DRIVES];

static inline struct partition *ata_get_device(devminor_t minor)
{
	short drive = minor >> 4;
	minor &= 0xf;

	if (drive >= ATA_MAX_DRIVES || minor > PARTITION_MAX || drives[drive].parts[minor].base == 0)
		return NULL;
	return &drives[drive].parts[minor];
}


int ata_init()
{
	//*COMET_VME_CF_CONTROL = 0xb8;

	for (short i = 0; i < PARTITION_MAX; i++)
		drives[0].parts[i].base = 0;
	register_driver(DEVMAJOR_ATA, &ata_driver);

	// TODO this doesn't work very well
	if (!ata_detect()) {
		printk_safe("ATA device not detected\n");
		return 0;
	}

	char buffer[512];

	ata_read_sector(0, buffer);
	read_partition_table(buffer, drives[0].parts);

	for (short i = 0; i < PARTITION_MAX; i++) {
		if (drives[0].parts[i].size)
			printk_safe("ata%d: found partition with %d sectors\n", i, drives[0].parts[i].size);
	}

	return 0;
}

int ata_open(devminor_t minor, int access)
{
	if (!ata_get_device(minor))
		return ENXIO;
	return 0;
}

int ata_close(devminor_t minor)
{
	if (!ata_get_device(minor))
		return ENXIO;
	return 0;
}

int ata_read(devminor_t minor, char *buffer, offset_t offset, size_t size)
{
	struct partition *device = ata_get_device(minor);
	if (!device)
		return ENXIO;

	if (offset > (device->size << 9))
		return -1;
	if (offset + size > (device->size << 9))
		size = (device->size << 9) - offset;

	offset >>= 9;
	for (int count = size >> 9; count > 0; count--, offset++, buffer = &buffer[512])
		ata_read_sector(device->base + offset, buffer);
	return size;
}

int ata_write(devminor_t minor, const char *buffer, offset_t offset, size_t size)
{
	struct partition *device = ata_get_device(minor);
	if (!device)
		return ENXIO;

	if (offset > (device->size << 9))
		return -1;
	if (offset + size > (device->size << 9))
		size = (device->size << 9) - offset;

	offset >>= 9;
	for (int count = size >> 9; count > 0; count--, offset++, buffer = &buffer[512])
		ata_write_sector(device->base + offset, buffer);
	return size;
}

int ata_ioctl(devminor_t minor, unsigned int request, void *argp, uid_t uid)
{
	struct partition *device = ata_get_device(minor);
	if (!device)
		return ENXIO;
	return -1;
}

int ata_poll(devminor_t minor, int events)
{
	// TODO this could maybe do a hardware check to make sure the drive is accessible?
	return events & (VFS_POLL_READ | VFS_POLL_WRITE);
}

offset_t ata_seek(devminor_t minor, offset_t position, int whence, offset_t offset)
{
	struct partition *device = ata_get_device(minor);
	if (!device)
		return ENXIO;

	switch (whence) {
	    case SEEK_SET:
		break;
	    case SEEK_CUR:
		position = offset + position;
		break;
	    case SEEK_END:
		position = device->size + position;
		break;
	    default:
		return EINVAL;
	}

	if (position > device->size)
		position = device->size;
	return position;

}
