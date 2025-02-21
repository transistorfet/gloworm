
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <kernel/vfs.h>
#include <kernel/time.h>
#include <kernel/kmalloc.h>
#include <kernel/syscall.h>
#include <kernel/driver.h>
#include <kernel/printk.h>
#include <kernel/scheduler.h>
#include <kernel/kconfig.h>

#include "proc/tasks.h"
#include "proc/process.h"

#if defined(CONFIG_NET)
#include "net/if.h"
#include "net/protocol.h"
#endif

#include "api.h"
#include "interrupts.h"


extern void tty_68681_preinit();

extern struct driver tty_68681_driver;
extern struct driver tty_driver;
extern struct driver mem_driver;
extern struct driver ata_driver;

struct driver *drivers[] = {
	#if defined(CONFIG_TTY)
	#if defined(CONFIG_TTY_68681)
	&tty_68681_driver,
	#endif
	&tty_driver,
	#endif
	#if defined(CONFIG_MEM)
	&mem_driver,
	#endif
	#if defined(CONFIG_ATA)
	&ata_driver,
	#endif
	NULL	// Null Termination
};

extern struct mount_ops mallocfs_mount_ops;
extern struct mount_ops minix_mount_ops;
extern struct mount_ops procfs_mount_ops;

struct mount_ops *filesystems[] = {
	//&mallocfs_mount_ops,
	#if defined(CONFIG_MINIX_FS)
	&minix_mount_ops,
	#endif
	&procfs_mount_ops,
	NULL	// Null Termination
};

#if defined(CONFIG_NET)
extern struct if_ops slip_if_ops;

struct if_ops *interfaces[] = {
	#if defined(CONFIG_SLIP)
	&slip_if_ops,
	#endif
	NULL	// Null Termination
};

extern struct protocol_ops ipv4_protocol_ops;
extern struct protocol_ops icmp_protocol_ops;
extern struct protocol_ops udp_protocol_ops;
extern struct protocol_ops tcp_protocol_ops;

struct protocol_ops *protocols[] = {
	&ipv4_protocol_ops,
	&icmp_protocol_ops,
	&udp_protocol_ops,
	&tcp_protocol_ops,
	NULL	// Null Termination
};
#endif


char boot_args[32] = "ata0";
device_t root_dev = DEVNUM(DEVMAJOR_MEM, 0);


void create_dir_or_panic(const char *path)
{
	struct vfile *file;
	struct vnode *vnode;

	if (!vfs_lookup(NULL, path, SU_UID, VLOOKUP_NORMAL, &vnode)) {
		vfs_release_vnode(vnode);
	} else {
		if (vfs_open(NULL, path, O_CREAT, S_IFDIR | 0755, SU_UID, &file))
			panic("Unable to create %s\n", path);
		vfs_close(file);
	}
}

void create_special_or_panic(const char *path, device_t rdev)
{
	struct vnode *vnode;

	if (vfs_lookup(NULL, path, SU_UID, VLOOKUP_NORMAL, &vnode)) {
		if (vfs_mknod(NULL, path, S_IFCHR | 0755, rdev, SU_UID, &vnode))
			panic("Unable to create special file %s\n", path);
	}
	vfs_release_vnode(vnode);
}

void parse_boot_args()
{
	// TODO this is overly simplistic because there aren't many options yet
	if (!*boot_args) {
		root_dev = DEVNUM(DEVMAJOR_MEM, 0);
	}

	if (!strncmp(boot_args, "mem", 3)) {
		root_dev = DEVNUM(DEVMAJOR_MEM, boot_args[3] - '0');
	} else if (!strncmp(boot_args, "ata", 3)) {
		root_dev = DEVNUM(DEVMAJOR_ATA, boot_args[3] - '0');
	}
}

int main()
{
	DISABLE_INTS();

	tty_68681_preinit();

	printk_safe("\nBooting with \"%s\"...\n\n", boot_args);
	parse_boot_args();

	//init_kernel_heap((void *) 0x110000, 0xD0000);
	init_kernel_heap((void *) 0x120000, 0xC0000);

	init_time();
	init_timer_list();
	init_interrupts();
	init_syscall();
	init_proc();
	init_scheduler();

	// Initialize drivers before VFS
	for (short i = 0; drivers[i]; i++)
		drivers[i]->init();

	init_vfs();

	// Initialize specific filesystems
	for (short i = 0; filesystems[i]; i++)
		filesystems[i]->init();

	// Initialize the networking subsystem
	#if defined(CONFIG_NET)
	init_net_if();
	init_net_protocol();

	// Initialize specific network interfaces
	for (short i = 0; interfaces[i]; i++)
		interfaces[i]->init();

	// Initialize specific network protocols
	for (short i = 0; protocols[i]; i++)
		protocols[i]->init();
	#endif

	// TODO hack because something isn't working
	root_dev = DEVNUM(DEVMAJOR_ATA, 0);

	printk_safe("minixfs: mounting (%x) at %s\n", root_dev, "/");
	vfs_mount(NULL, "/", root_dev, &minix_mount_ops, 0, SU_UID);


	// TODO this would be moved elsewhere
	create_dir_or_panic("/bin");
	create_dir_or_panic("/dev");
	create_dir_or_panic("/proc");
	create_dir_or_panic("/media");

	create_special_or_panic("/dev/tty0", DEVNUM(DEVMAJOR_TTY, 0));
	create_special_or_panic("/dev/tty1", DEVNUM(DEVMAJOR_TTY, 1));
	create_special_or_panic("/dev/mem0", DEVNUM(DEVMAJOR_MEM, 0));
	create_special_or_panic("/dev/ata0", DEVNUM(DEVMAJOR_ATA, 0));

	// TODO device number here is an issue because 0 is used to indicated a mount slot is not used, which when mounting after this causes a /proc error
	printk_safe("procfs: mounting at /proc\n");
	vfs_mount(NULL, "/proc", 1, &procfs_mount_ops, VFS_MBF_READ_ONLY, SU_UID);

	//vfs_mount(NULL, "/media", DEVNUM(DEVMAJOR_ATA, 0), &minix_mount_ops, SU_UID);


	#if defined(CONFIG_NET)
	// TODO this is a temporary hack.  The ifup should be done through ifconfig
	struct if_device *ifdev = net_if_find("slip0", NULL);
	memset(&ifdev->address, '\0', sizeof(struct sockaddr_in));
	((struct sockaddr_in *) &ifdev->address)->sin_family = AF_INET;
	inet_aton("192.168.1.200", &((struct sockaddr_in *) &ifdev->address)->sin_addr);
	net_if_change_state(ifdev, IFF_UP);
	#endif

	create_init_task();

	begin_multitasking();
}

