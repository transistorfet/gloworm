
#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#include <stddef.h>
#include <sys/types.h>

#define PROT_READ		0x0001		// page can be read
#define PROT_WRITE		0x0002		// page can be written
#define PROT_EXEC		0x0004		// page can be executed
#define PROT_NONE		0x0000		// page cannot be accessed

#define MAP_SHARED		0x000001	// Share changes
#define MAP_PRIVATE		0x000002	// Changes are private

#define MAP_TYPE		0x00000F	// Mask for type of mapping
#define MAP_FIXED		0x000010	// Interpret addr exactly
#define MAP_ANONYMOUS		0x000020	// don't use a file
#define MAP_POPULATE		0x008000	// populate (prefault) pagetables
#define MAP_NONBLOCK		0x010000	// do not block on IO
#define MAP_STACK		0x020000	// give out an address that is best suited for process/thread stacks
#define MAP_HUGETLB		0x040000	// create a huge page mapping
#define MAP_SYNC		0x080000	// perform synchronous page faults for the mapping
#define MAP_FIXED_NOREPLACE	0x100000	// MAP_FIXED which doesn't unmap underlying mapping
#define MAP_UNINITIALIZED	0x4000000	// For anonymous mmap, memory could be


void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);

#endif

