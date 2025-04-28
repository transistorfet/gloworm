/*
 * Bootloader - Given a zone number to the inode table and an inode offset into that table, look up the zone
 *		data and indirect zone table and load the kernel zones from a minix disk in onboard flash or
 *		compact flash card.  It currently must be compiled specially for the image location
 */

#include <stddef.h>
#include <string.h>

#include <endian.h>
#include <kconfig.h>

#include "../kernel/fs/minix/minix-v1.h"

#define PARTITION_OFFSET	0x1BE

struct partition_entry {
	uint8_t status;
	uint8_t chs_start[3];
	uint8_t fstype;
	uint8_t chs_end[3];
	uint32_t lba_start;
	uint32_t lba_sectors;
};

struct boot_drive {
	char drive_type;				// 1 = ROM, 2 = ATA
	char partition;					// The partition number of the boot partition
	int ata_lba_start;				// The sector number of the start of the boot partition
};

static char *load_address = (char *) 0x100000;		// Address to load the kernel
static char *mem_drive = (char *) 0x080000;		// The address in ROM of the start of the mem drive
static const short inode_num = 2;			// Inode offset into zone of the kernel inode


int init_tty();
int putchar(int ch);
void load_partition(struct boot_drive *drive);
void load_kernel(struct boot_drive *drive, char *offset);
int ata_read_sector(int sector, char *buffer);

int start(char *boot_args)
{
	struct boot_drive drive;

	if (!boot_args[0])
		boot_args = "ata0";

	drive.drive_type = boot_args[0] == 'a' ? 2 : 1;
	drive.partition = boot_args[3] - '0';
	drive.ata_lba_start = 0;

	init_tty();
	load_partition(&drive);
	load_kernel(&drive, load_address);

	void (*entry)(char *) = (void (*)(char *)) load_address;
	entry(boot_args);
	__builtin_unreachable();
}

char *copy_zone_data(struct boot_drive *drive, char *dest, minix_v1_zone_t zone)
{
	if (drive->drive_type == 1) {
		char *src = mem_drive + (zone * MINIX_V1_ZONE_SIZE);
		memcpy(dest, src, MINIX_V1_ZONE_SIZE);
	} else {
		ata_read_sector(drive->ata_lba_start + (zone << 1), dest);
	}

	return dest;
}

char *load_zones(struct boot_drive *drive, minix_v1_zone_t *zones, int max, char *dest)
{
	for (short i = 0; i < max; i++) {
		if (!zones[i])
			return NULL;
		copy_zone_data(drive, dest, le16toh(zones[i]));
		dest += MINIX_V1_ZONE_SIZE;
		putchar('.');
	}
	return dest;
}

void load_partition(struct boot_drive *drive)
{
	struct partition_entry *entry;
	char buffer[MINIX_V1_ZONE_SIZE];

	copy_zone_data(drive, buffer, 0);
	entry = (struct partition_entry *) &buffer[PARTITION_OFFSET];
	drive->ata_lba_start = le32toh(entry[(short) drive->partition].lba_start);
}

void load_kernel(struct boot_drive *drive, char *offset)
{
	minix_v1_zone_t *zone_table;
	minix_v1_zone_t *inode_zones;
	struct minix_v1_superblock *super;
	struct minix_v1_inode *inode_table;
	char buffer[MINIX_V1_ZONE_SIZE];

	// Load our target inode and get the zone table in the inode
	super = (struct minix_v1_superblock *) copy_zone_data(drive, buffer, MINIX_V1_SUPER_ZONE);
	//MINIX_V1_INODE_TABLE_START(super);
	minix_v1_zone_t inode_zone = MINIX_V1_BITMAP_ZONES + le16toh((super)->imap_blocks) + le16toh((super)->zmap_blocks);

	// Load our target inode and get the zone table in the inode
	inode_table = (struct minix_v1_inode *) copy_zone_data(drive, buffer, inode_zone);
	inode_zones = inode_table[inode_num - 1].zones;

	// Copy all the inode zones to RAM
	offset = load_zones(drive, inode_zones, MINIX_V1_TIER1_ZONENUMS, offset);

	// If there are more zones, load the indirect zone table and copy all the zones to RAM
	if (offset && inode_zones[MINIX_V1_TIER1_ZONENUMS]) {
		zone_table = (minix_v1_zone_t *) copy_zone_data(drive, buffer, le16toh(inode_table[inode_num - 1].zones[MINIX_V1_TIER1_ZONENUMS]));
		offset = load_zones(drive, zone_table, MINIX_V1_ZONENUMS_PER_ZONE, offset);
	}

	putchar('\n');
	// Delay to let the last character get written
	for (short i = 0; i < 1000; i++) { asm volatile(""); }
}

#define ATA_REG_BASE		CONFIG_ATA_BASE
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
#define ATA_WAIT()		{ while (*ATA_REG_STATUS & ATA_ST_BUSY) { } }
#define ATA_WAIT_FOR_DATA()	{ while (!((*ATA_REG_STATUS) & ATA_ST_DATA_READY)) { } }

void ata_wait()
{
	ATA_DELAY(10);
	ATA_WAIT();
	ATA_DELAY(10);
}

static inline char hexchar(uint8_t byte)
{
	if (byte < 10) {
		return byte + 0x30;
	} else {
		return byte + 0x37;
	}
}

int ata_read_sector(int sector, char *buffer)
{
	// Set 8-bit mode
	(*ATA_REG_FEATURE) = 0x01;
	(*ATA_REG_COMMAND) = ATA_CMD_SET_FEATURE;
	ata_wait();

	// Read a sector
	(*ATA_REG_DRIVE_HEAD) = 0xE0;
	//(*ATA_REG_DRIVE_HEAD) = 0xE0 | (uint8_t) ((sector >> 24) & 0x0F);
	(*ATA_REG_SECTOR_NUM) = (uint8_t) sector;
	(*ATA_REG_CYL_LOW) = (uint8_t) (sector >> 8);
	(*ATA_REG_CYL_HIGH) = (uint8_t) (sector >> 16);
	(*ATA_REG_SECTOR_COUNT) = 2;
	(*ATA_REG_COMMAND) = ATA_CMD_READ_SECTORS;
	ata_wait();

	char status = (*ATA_REG_STATUS);
	if (status & 0x01)
		return 0;

	ata_wait();
	ATA_WAIT_FOR_DATA();

	for (int i = 0; i < 1024; i++) {
		buffer[i] = (*ATA_REG_DATA_BYTE);
		ata_wait();
	}

	/*
	for (int i = 0; i < 1024; i++) {
		putchar(hexchar((buffer[i] >> 4) & 0xF));
		putchar(hexchar(buffer[i] & 0xF));
		if ((i & 0x1F) == 0x1F)
			putchar('\n');
	}
	*/

	return 0;
}

// MC68681 Register Addresses
#define MR1A_MR2A_ADDR	((volatile uint8_t *) 0x700001)
#define SRA_RD_ADDR	((volatile uint8_t *) 0x700003)
#define CSRA_WR_ADDR	((volatile uint8_t *) 0x700003)
#define CRA_WR_ADDR	((volatile uint8_t *) 0x700005)
#define TBA_WR_ADDR	((volatile uint8_t *) 0x700007)
#define RBA_RD_ADDR	((volatile uint8_t *) 0x700007)
#define ACR_WR_ADDR	((volatile uint8_t *) 0x700009)

#define CTUR_WR_ADDR	((volatile uint8_t *) 0x70000D)
#define CTLR_WR_ADDR	((volatile uint8_t *) 0x70000F)
#define START_RD_ADDR	((volatile uint8_t *) 0x70001D)
#define STOP_RD_ADDR	((volatile uint8_t *) 0x70001F)

#define IPCR_RD_ADDR	((volatile uint8_t *) 0x700009)
#define OPCR_WR_ADDR	((volatile uint8_t *) 0x70001B)
#define INPUT_RD_ADDR	((volatile uint8_t *) 0x70001B)
#define OUT_SET_ADDR	((volatile uint8_t *) 0x70001D)
#define OUT_RESET_ADDR	((volatile uint8_t *) 0x70001F)

#define ISR_RD_ADDR	((volatile uint8_t *) 0x70000B)
#define IMR_WR_ADDR	((volatile uint8_t *) 0x70000B)
#define IVR_WR_ADDR	((volatile uint8_t *) 0x700019)


// MC68681 Command Numbers
#define CMD_RESET_MR			0x10
#define CMD_RESET_RX			0x20
#define CMD_RESET_TX			0x30
#define CMD_RESET_ERROR			0x40
#define CMD_ENABLE_TX_RX		0x05
#define CMD_ENABLE_RX			0x01
#define CMD_DISABLE_RX			0x02
#define CMD_ENABLE_TX			0x04
#define CMD_DISABLE_TX			0x08
#define CMD_START_BREAK			0x60
#define CMD_STOP_BREAK			0x70


// MC68681 Default Configuration Values
#define MR1A_MODE_A_REG_1_CONFIG	0b10010011	// RxRTS Enabled, 8 bits, No Parity
#define MR2A_MODE_A_REG_2_CONFIG	0b00000111	// Normal mode, CTS Disabled, 1 stop bit
//#define CSRA_CLK_SELECT_REG_A_CONFIG	0b10111011	// 9600 bps @ 3.6864MHz (19200 @ 7.3728 MHz)
//#define ACR_AUX_CONTROL_REG_CONFIG	0b11111000	// Set2, External Clock / 16, IRQs disabled
#define CSRA_CLK_SELECT_REG_A_CONFIG	0b11001100	// 38400 bps @ 3.6864MHz
#define ACR_AUX_CONTROL_REG_CONFIG	0b01111000	// Set1, External Clock / 16, IRQs disabled except IP3


// Status Register Bits (SRA/SRB)
#define SR_RECEIVED_BREAK		0x80
#define SR_FRAMING_ERROR		0x40
#define SR_PARITY_ERROR			0x20
#define SR_OVERRUN_ERROR		0x10
#define SR_TX_EMPTY			0x08
#define SR_TX_READY			0x04
#define SR_RX_FULL			0x02
#define SR_RX_READY			0x01


int init_tty()
{
	// Disable interrupts
	*IMR_WR_ADDR = 0x00;

	*CRA_WR_ADDR = CMD_RESET_MR;
	*CRA_WR_ADDR = CMD_RESET_TX;

	*MR1A_MR2A_ADDR = MR1A_MODE_A_REG_1_CONFIG;
	*MR1A_MR2A_ADDR = MR2A_MODE_A_REG_2_CONFIG;
	*CSRA_WR_ADDR = CSRA_CLK_SELECT_REG_A_CONFIG;
	*ACR_WR_ADDR = ACR_AUX_CONTROL_REG_CONFIG;

	*CRA_WR_ADDR = CMD_ENABLE_TX;
	return 0;
}

int putchar(int ch)
{
	while (!(*SRA_RD_ADDR & SR_TX_READY)) { }
	*TBA_WR_ADDR = (char) ch;
	return ch;
}

