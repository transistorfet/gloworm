/*
 * Monitor - the program in onboard flash which initially boots the processor, and provides some basic serial transfer commands
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <kconfig.h>

#if defined(CONFIG_AM29F040)
#define BOARD	"k30"
#elif defined(CONFIG_DUAL_AM29F040)
#define BOARD	"68k"
#else
#error "Board type not set"
#endif

#define VERSION		"2025-01-31-" BOARD


extern void init_tty();
extern void spin_loop(int count);
char *led = (char *) 0x201c;

int readline(char *buffer, short max)
{
	short i = 0;

	while (i < max) {
		buffer[i] = getchar();
		switch (buffer[i]) {
			case 0x08: {
				if (i >= 1) {
					putchar(0x08);
					putchar(' ');
					putchar(0x08);
					i--;
				}
				break;
			}
			case '\n': {
				putchar(buffer[i]);
				buffer[i] = '\0';
				return i;
			}
			default: {
				putchar(buffer[i++]);
				break;
			}
		}
	}
	return i;
}

int parseline(char *input, char **vargs)
{
	short j = 0;

	while (*input == ' ')
		input++;

	vargs[j++] = input;
	while (*input != '\0' && *input != '\n' && *input != '\r') {
		if (*input == ' ') {
			*input = '\0';
			input++;
			while (*input == ' ')
				input++;
			vargs[j++] = input;
		} else
			input++;
	}

	*input = '\0';
	if (*vargs[j - 1] == '\0')
		j -= 1;
	vargs[j] = NULL;

	return j;
}

char hexchar(uint8_t byte)
{
	if (byte < 10) {
		return byte + 0x30;
	} else {
		return byte + 0x37;
	}
}

void dump(const uint16_t *addr, short len)
{
	char buffer[6];

	len >>= 1; // Adjust the dump size from bytes to words
	buffer[4] = ' ';
	buffer[5] = '\0';
	while (len > 0) {
		printf("%x: ", (unsigned int) addr);
		for (short i = 0; i < 8 && len > 0; i++, len--) {
			buffer[0] = hexchar((addr[i] >> 12) & 0xF);
			buffer[1] = hexchar((addr[i] >> 8) & 0xF);
			buffer[2] = hexchar((addr[i] >> 4) & 0xF);
			buffer[3] = hexchar(addr[i] & 0x0F);
			fputs(buffer, stdout);
		}
		putchar('\n');
		addr += 8;
	}
	putchar('\n');
}



/************
 * Commands *
 ************/

#define ROM_ADDR	0x000000
#define ROM_SIZE	0x1800

#define RAM_ADDR	0x200000
#define RAM_SIZE	0x100000

#define SUPERVISOR_ADDR	0x200000


void command_version(int argc, char **args)
{
	printf("version: %s\n", VERSION);
	return;
}

void command_info(int argc, char **args)
{
	uint32_t pc;
	uint32_t sp;
	uint32_t sv1;
	uint16_t flags;

	asm(
	"move.l	%%sp, %0\n"
	"move.l	(%%sp), %1\n"
	"move.w	%%sr, %2\n"
	"bsr.w	INFO\n"
	"INFO:\t"
	"move.l	(%%sp)+, %3\n"
	: "=r" (sp), "=r" (sv1), "=r" (flags), "=r" (pc)
	);

	printf("PC: %x\n", pc);
	printf("SR: %x\n", flags);
	printf("SP: %x\n", sp);
	printf("TOP: %x\n", sv1);
	return;
}

void command_dump(int argc, char **args)
{
	if (argc <= 1) {
		fputs("You need an address\n", stdout);
	} else {
		short length = 0x40;

		if (argc >= 3)
			length = strtol(args[2], NULL, 16);
		dump((const uint16_t *) strtol(args[1], NULL, 16), length);
	}
}

void command_dumpram(int argc, char **args)
{
	dump((uint16_t *) RAM_ADDR, 0x1800);
}

void command_move(int argc, char **args)
{
	if (argc <= 3) {
		fputs("Must supply a source, destination, and size arguments in hex\n", stdout);
	} else {
		uint16_t *source = (uint16_t *) strtol(args[1], NULL, 16);
		uint16_t *dest = (uint16_t *) strtol(args[2], NULL, 16);
		uint32_t size = strtol(args[3], NULL, 16);

		for (int i = 0; i < (size >> 1); i++) {
			dest[i] = source[i];
		}

		printf("%d (%x) bytes transferred\n", size, size);
	}
}

void command_poke(int argc, char **args)
{
	if (argc <= 2) {
		fputs("You need an address and byte to poke\n", stdout);
	} else {
		uint8_t *address = (uint8_t *) strtol(args[1], NULL, 16);
		uint8_t data = (uint8_t) strtol(args[2], NULL, 16);
		*(address) = data;
	}
}

void command_peek(int argc, char **args)
{
	if (argc <= 1) {
		fputs("You need an address\n", stdout);
	} else {
		uint8_t *address = (uint8_t *) strtol(args[1], NULL, 16);
		//uint8_t data = (uint8_t) strtol(args[2], NULL, 16);
		//*(address) = data;
		uint8_t data = *(address);
		printf("%x\n", data);
	}
}

void erase_flash(uint32_t sector)
{
	#if defined(CONFIG_AM29F040)
	printf("Erasing flash sector %d", sector);
	*((volatile uint8_t *) 0x555) = 0xAA;
	putchar('.');
	*((volatile uint8_t *) 0x2AA) = 0x55;
	putchar('.');
	*((volatile uint8_t *) 0x555) = 0x80;
	putchar('.');
	*((volatile uint8_t *) 0x555) = 0xAA;
	putchar('.');
	*((volatile uint8_t *) 0x2AA) = 0x55;
	putchar('.');
	*((volatile uint8_t *) sector) = 0x30;
	putchar('.');
	#elif defined(CONFIG_DUAL_AM29F040)
	printf("Erasing flash sector %d", sector);
	*((volatile uint16_t *) (0x555 << 1)) = 0xAAAA;
	putchar('.');
	*((volatile uint16_t *) (0x2AA << 1)) = 0x5555;
	putchar('.');
	*((volatile uint16_t *) (0x555 << 1)) = 0x8080;
	putchar('.');
	*((volatile uint16_t *) (0x555 << 1)) = 0xAAAA;
	putchar('.');
	*((volatile uint16_t *) (0x2AA << 1)) = 0x5555;
	putchar('.');
	*((volatile uint16_t *) sector) = 0x3030;
	putchar('.');
	#endif
}

#if defined(CONFIG_AM29F040)
#define SECTOR_SIZE	0x010000
#elif defined(CONFIG_DUAL_AM29F040)
#define SECTOR_SIZE	0x020000
#else
#error "No board type given"
#endif

void command_eraserom(int argc, char **args)
{
	uint16_t data;
	uint16_t *dest = (uint16_t *) ROM_ADDR;
	uint32_t sector = 0;

	if (argc >= 2) {
		sector = strtol(args[1], NULL, 16);
		if ((sector & (SECTOR_SIZE - 1)) || (sector >= RAM_ADDR)) {
			printf("Invalid sector address to erase (%x)\n", sector);
			return;
		}
		dest = (uint16_t *) sector;
	}

	erase_flash(sector);
	spin_loop(600000);
	data = dest[0];

	fputs("\nVerifying erase\n\n", stdout);
	for (int i = 0; i < (ROM_SIZE >> 1); i++) {
		data = dest[i];
		if (data != 0xFFFF) {
			printf("Flash not erased at %lx (%x)\n", (unsigned long) (dest + i), data);
			return;
		}
	}

	fputs("Rom erased! Make sure to writerom before resetting\n\n", stdout);
}


void program_flash_data(uint16_t *addr, uint16_t data)
{
	#if defined(CONFIG_AM29F040)
	*((volatile uint8_t *) 0x555) = 0xAA;
	*((volatile uint8_t *) 0x2AA) = 0x55;
	*((volatile uint8_t *) 0x555) = 0xA0;
	*((volatile uint8_t *) addr) = (uint8_t) (data >> 8);
	spin_loop(200);
	*((volatile uint8_t *) 0x555) = 0xAA;
	*((volatile uint8_t *) 0x2AA) = 0x55;
	*((volatile uint8_t *) 0x555) = 0xA0;
	*(((volatile uint8_t *) addr) + 1) = (uint8_t) (data & 0xFF);
	#elif defined(CONFIG_DUAL_AM29F040)
	*((volatile uint16_t *) (0x555 << 1)) = 0xAAAA;
	*((volatile uint16_t *) (0x2AA << 1)) = 0x5555;
	*((volatile uint16_t *) (0x555 << 1)) = 0xA0A0;
	*((volatile uint16_t *) addr) = data;
	spin_loop(200);
	#endif
}

void command_writerom(int argc, char **args)
{
	uint16_t data;

	uint16_t *dest = (uint16_t *) ROM_ADDR;
	uint16_t *source = (uint16_t *) RAM_ADDR;

	if (argc >= 2)
		dest = (uint16_t *) strtol(args[1], NULL, 16);
	if (argc >= 3)
		source = (uint16_t *) strtol(args[2], NULL, 16);

	for (int i = 0; i < (ROM_SIZE >> 1); i++) {
		data = dest[i];
		if (data != 0xFFFF) {
			printf("Flash not erased at %lx (%x)\n", (unsigned long) (dest + i), data);
			return;
		}
	}

	for (int i = 0; i < (ROM_SIZE >> 1); i++) {
		program_flash_data(&dest[i], source[i]);
		spin_loop(200);
		printf("%x ", dest[i]);
	}

	fputs("\nWrite complete\n", stdout);
}

void command_verifyrom(int argc, char **args)
{
	uint16_t errors = 0;
	uint32_t size = ROM_SIZE;

	uint16_t *source = (uint16_t *) RAM_ADDR;
	uint16_t *dest = (uint16_t *) ROM_ADDR;

	if (argc >= 2)
		dest = (uint16_t *) strtol(args[1], NULL, 16);
	if (argc >= 3)
		source = (uint16_t *) strtol(args[2], NULL, 16);
	if (argc >= 4)
		size = strtol(args[3], NULL, 16);

	for (int i = 0; i < (size >> 1); i++) {
		if (dest[i] != source[i]) {
			printf("@%lx expected %x but found %x\n", (unsigned long) &dest[i], source[i], dest[i]);
			if (++errors > 100) {
				fputs("Bailing out\n", stdout);
				break;
			}
		}
	}

	fputs("\nVerification complete\n", stdout);
}

uint16_t fetch_word(char max)
{
	char buffer[4];

	for (short i = 0; i < max; i++) {
		buffer[i] = getchar();
		buffer[i] = buffer[i] <= '9' ? buffer[i] - 0x30 : buffer[i] - 0x37;
	}

	return (buffer[0] << 12) | (buffer[1] << 8) | (buffer[2] << 4) | buffer[3];
}

void command_load(int argc, char **args)
{
	int i;
	char odd_size;
	uint32_t size;
	uint16_t data;
	uint16_t *mem = (uint16_t *) RAM_ADDR;

	size = fetch_word(4);
	odd_size = size & 0x01;
	size >>= 1;
	//printf("Expecting %x\n", size);

	if (argc >= 2)
		mem = (uint16_t *) strtol(args[1], NULL, 16);

	for (i = 0; i < size; i++) {
		data = fetch_word(4);
		//printf("%x ", data);
		mem[i] = data;
	}

	if (odd_size)
		mem[i] = fetch_word(2);

	fputs("Load complete\n", stdout);
}

void command_boot(int argc, char **args)
{
	char *boot_args = "";
	void (*entry)(char *) = (void (*)(char *)) RAM_ADDR;

	if (argc >= 2)
		entry = (void (*)(char *)) strtol(args[1], NULL, 16);
	if (argc >= 3)
		boot_args = args[2];
	((void (*)(char *)) entry)(boot_args);
}


void command_ramtest(int argc, char **args)
{
	uint16_t data;
	uint16_t errors = 0;

	uint16_t *dest = (uint16_t *) RAM_ADDR;
	uint32_t length = RAM_SIZE;

	if (argc >= 2)
		dest = (uint16_t *) strtol(args[1], NULL, 16);
	if (length >= 3)
		length = strtol(args[2], NULL, 16);

	printf("Testing onboard ram from %x to %x\n", (uint32_t) dest, (uint32_t) dest + length);

	uint16_t *mem = (uint16_t *) dest;
	for (int i = 0; i < (length >> 1); i++) {
		mem[i] = (uint16_t) i;
	}

	//uint8_t *mem2 = (uint8_t *) dest;
	//for (int i = 0; i < length; i++) {
	//	printf("%x ", (uint8_t) mem2[i]);
	//}


	for (int i = 0; i < (length >> 1); i++) {
		data = (uint16_t) mem[i];
		if (data != (uint16_t) i) {
			printf("%08x: %x\n", (unsigned int) &mem[i], data);
			errors++;
		}
	}

	printf("\nErrors: %d\n", errors);
}

void command_vmetest(int argc, char **args)
{
	// Test different VME bus accesses
	//volatile uint16_t *vme_address = (uint16_t *) 0xff8ABCDE;
	// *vme_address = (uint16_t) 0xAABB;
	// *vme_address = 0xaa55;
	// *vme_address = (uint16_t) ((uint32_t) vme_address >> 12);
	// *((uint8_t *) vme_address) = 0xaa;
	// *((uint32_t *) vme_address) = 0xaa55bb44;
	// *((uint8_t *) vme_address) = 0xaa;
	// *((uint32_t *) vme_address) = 0xaa55bb44;
	//printf("%x: %x\n", vme_address, *vme_address);


	volatile uint8_t *vme_address_byte = (uint8_t *) 0xFF800020;
	printf("%lx: %x\n", (unsigned long) vme_address_byte, *vme_address_byte);
	//*vme_address_byte = 0x00;

	/*
	for (int i = 0; i < 0xffff; i += 2)
	{
		uint16_t expected = (uint16_t) (i & 0xFF);
		vme_address_word[i >> 1] = (uint16_t) ((expected << 8) | ((expected + 1) & 0xFF));
	}
	*/

	/*
	volatile uint8_t *vme_address_byte = (uint8_t *) 0xFFC00000;
	volatile uint16_t *vme_address_word = (uint16_t *) 0xFFC00000;
	for (int i = 0; i < 0xffff; i++)
	{
		vme_address_byte[i] = (uint8_t) i;
	}

	for (int i = 0; i < 0xffff; i += 2)
	{
		uint16_t expected = (uint16_t) (i & 0xFF);
		uint16_t data = vme_address_word[i >> 1];
		if (data != ((expected << 8) | ((expected + 1) & 0xFF))) {
			printf("%08x: %04x\n", &vme_address_byte[i], data);
		}
	}
	*/
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
#define ATA_WAIT()		{ ATA_DELAY(4); while (*ATA_REG_STATUS & ATA_ST_BUSY) { } }
#define ATA_WAIT_FOR_DATA()	{ while (!((*ATA_REG_STATUS) & ATA_ST_DATA_READY)) { } }

void command_atatest(int argc, char **args)
{
	uint32_t sector = 46842;
	char buffer[512];

	if (argc >= 2)
		sector = (uint32_t) strtol(args[1], NULL, 10);

	//printf("Set COMET CompactFlash async mode\n");
	//*COMET_VME_CF_CONTROL = 0x00;
	//ATA_DELAY(10);
	//*COMET_VME_CF_CONTROL = 0xb8;
	//ATA_DELAY(20);

	// Set 8-bit mode
	//printf("Set 8-bit mode\n");
	(*ATA_REG_FEATURE) = 0x01;
	(*ATA_REG_COMMAND) = ATA_CMD_SET_FEATURE;
	ATA_WAIT();

	// Read a sector
	//printf("Setup read\n");
	(*ATA_REG_DRIVE_HEAD) = 0xE0;
	//(*ATA_REG_DRIVE_HEAD) = 0xE0 | (uint8_t) ((sector >> 24) & 0x0F);
	(*ATA_REG_CYL_HIGH) = (uint8_t) (sector >> 16);
	(*ATA_REG_CYL_LOW) = (uint8_t) (sector >> 8);
	(*ATA_REG_SECTOR_NUM) = (uint8_t) sector;
	(*ATA_REG_SECTOR_COUNT) = 1;
	(*ATA_REG_COMMAND) = ATA_CMD_READ_SECTORS;
	ATA_WAIT();

	//printf("Read status\n");
	char status = (*ATA_REG_STATUS);
	if (status & 0x01) {
		printf("Error while reading ata: %x\n", (*ATA_REG_ERROR));
		return;
	}

	//printf("Wait for data\n");
	ATA_WAIT();
	ATA_WAIT_FOR_DATA();

	//printf("Read data\n");
	for (int i = 0; i < 512; i++) {
		//((uint16_t *) buffer)[i] = (*ATA_REG_DATA);
		//asm volatile("rol.w	#8, %0\n" : "+g" (((uint16_t *) buffer)[i]));
		buffer[i] = (*ATA_REG_DATA_BYTE);
		//printf("%x ", 0xff & buffer[i]);

		//ATA_WAIT_FOR_DATA();
		ATA_WAIT();
		//ATA_DELAY(10);
	}

	printf("Mem %x:\n", sector);
	for (int i = 0; i < 512; i++) {
		printf("%02x ", 0xff & buffer[i]);
		if ((i & 0x1F) == 0x1F)
			printf("\n");
	}

	return;
}

/**************************
 * Command Line Execution *
 **************************/

struct command {
	char *name;
	void (*func)(int, char **);
};

#define add_command(n, f)	{			\
	command_list[num_commands].name = (n);		\
	command_list[num_commands++].func = (f);	\
}

int load_commands(struct command *command_list)
{
	int num_commands = 0;

	add_command("version", command_version);
	add_command("info", command_info);
	add_command("load", command_load);
	add_command("boot", command_boot);
	add_command("dump", command_dump);
	add_command("dumpram", command_dumpram);
	add_command("move", command_move);
	add_command("poke", command_poke);
	add_command("peek", command_peek);
	add_command("eraserom", command_eraserom);
	add_command("writerom", command_writerom);
	add_command("verifyrom", command_verifyrom);

	add_command("ramtest", command_ramtest);
	add_command("v", command_vmetest);
	add_command("a", command_atatest);

	return num_commands;
}

#define BUF_SIZE	100
#define ARG_SIZE	10

void serial_read_loop()
{
	int i;
	short argc;
	char buffer[BUF_SIZE];
	char *args[ARG_SIZE];

	struct command command_list[20];
	int num_commands = load_commands(command_list);

	while (1) {
		fputs("> ", stdout);
		readline(buffer, BUF_SIZE);
		argc = parseline(buffer, args);

		if (!strcmp(args[0], "test")) {
			fputs("this is only a test\n", stdout);
		} else if (!strcmp(args[0], "help")) {
			for (int i = 0; i < num_commands; i++) {
				fputs(command_list[i].name, stdout);
				putchar('\n');
			}
		} else {
			for (i = 0; i < num_commands; i++) {
				if (!strcmp(args[0], command_list[i].name)) {
					command_list[i].func(argc, args);
					break;
				}
			}

			if (i >= num_commands && args[0][0] != '\0') {
				fputs("Unknown command\n", stdout);
			}
		}
	}
}


#define ARDUINO_TRACE_ON()	asm volatile("movea.l	#0x2019, %%a0\n" "move.b	#1, (%%a0)" : : : "%a0");
#define ARDUINO_TRACE_OFF()	asm volatile("movea.l	#0x2019, %%a0\n" "move.b	#0, (%%a0)" : : : "%a0");

int main()
{
	//init_heap((void *) 0x101000, 0x1000);

	//*led = 0x1;

	init_tty();

	//ARDUINO_TRACE_OFF();

	//spin_loop(10000);

	fputs("\n\nWelcome to the k30-VME Monitor!\n\n", stdout);

	/*
	int *data = malloc(sizeof(int) * 10);
	data[0] = 0;
	data[1] = 1;
	data[2] = 2;
	printf("%x %x %x\n", &data[0], &data[1], &data[2]);
	printf("%d\n", *(data - 3));
	free(data);

	int *data2 = malloc(4);
	printf("%x %d\n", data2, *(data2 - 3));

	int *data3 = malloc(sizeof(int) * 2);
	printf("%x %d\n", data3, *(data3 - 3));

	free(data2);
	//printf("%x %d\n", data2, *(data2 - 3));
	print_free();

	int *data4 = malloc(sizeof(int) * 10);
	printf("%x %d\n", data4, *(data4 - 3));

	print_free();
	*/

	/*
	uint8_t *mem2 = (uint8_t *) 0x100000;
	for (short i = 0; i < 20; i++) {
		printf("%x ", (uint8_t) mem2[i]);
	}
	*/

	/*
	uint16_t *mem3 = (uint16_t *) 0x200000;
	for (int i = 0; i < 0x100; i++) {
		printf("%x ", (uint16_t) mem3[i]);
	}
	*/

	/*
	// Cause an address exception
	char test[10];
	*((short *) &test[1]) = 10;
	*/

	serial_read_loop();

	return 0;
}

