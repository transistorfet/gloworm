
#ifndef _SRC_ASM_MMU_H
#define _SRC_ASM_MMU_H

#include <stddef.h>
#include <stdint.h>

#include <kernel/mm/pages.h>


#define MMU_DT_INVALID		0
#define MMU_DT_PAGE_DESCRIPTOR	1
#define MMU_DT_TABLE_SHORT	2
#define MMU_DT_TABLE_LONG	3

#define MMU_STATUS_DESC_TYPE	0x0003
#define MMU_STATUS_DESC_WP	0x0004
#define MMU_STATUS_DESC_U	0x0008
#define MMU_STATUS_DESC_M	0x0010
#define MMU_STATUS_DESC_CI	0x0040
#define MMU_STATUS_DESC_S	0x0100
#define MMU_STATUS_DEFAULT	0xFC00

#define MMU_TC_ENABLE		0x10000000
#define MMU_TC_SRE		0x02000000
#define MMU_TC_FCL		0x01000000
#define MMU_TC_PAGE_SIZE	0x00F00000	/// Page size - the number of least significant bits that index a single page
#define MMU_TC_INITIAL		0x000F0000	/// Initial Shift - The number of most significant address bits that are ignored
#define MMU_TC_TIA		0x0000F000
#define MMU_TC_TIB		0x00000F00
#define MMU_TC_TIC		0x000000F0
#define MMU_TC_TID		0x0000000F

#define MMU_TC_PAGE_SIZE_SHIFT	20
#define MMU_TC_INITIAL_SHIFT	16
#define MMU_TC_TIA_SHIFT	12
#define MMU_TC_TIB_SHIFT	8
#define MMU_TC_TIC_SHIFT	4
#define MMU_TC_TID_SHIFT	0

typedef uint32_t virtual_address_t;
typedef uint32_t physical_address_t;

struct mmu_root_pointer {
	uint16_t limit;
	uint16_t status;
	uint32_t table;
};


//// Short Descriptors ////

#define MMU_TABLE_SIZE_SHORT		(PAGE_SIZE / sizeof(mmu_descriptor_short_t))
#define MMU_TABLE_ADDR_BITS_SHORT	(PAGE_ADDR_BITS - 2)
#define MMU_TABLE_LEVELS_SHORT		((32 - PAGE_ADDR_BITS) / MMU_TABLE_ADDR_BITS_SHORT)
#define MMU_TABLE_INITIAL_SHIFT_SHORT	((32 - PAGE_ADDR_BITS) % MMU_TABLE_ADDR_BITS_SHORT)

#define MMU_TABLE_ADDRESS_SHORT(desc)			(((uint32_t) (desc)) & ~(PAGE_SIZE - 1))
#define MMU_TABLE_STATUS_SHORT(desc)			(((uint32_t) (desc)) & (PAGE_SIZE - 1))
#define MMU_TABLE_DESCRIPTOR_SHORT(address, status)	((uint32_t) (((uint32_t) (address)) | ((uint32_t) (status))))

typedef uint32_t mmu_descriptor_short_t;
typedef mmu_descriptor_short_t mmu_table_short_t[MMU_TABLE_SIZE_SHORT];


//// Long Descriptors ////

#define MMU_TABLE_SIZE_LONG		(PAGE_SIZE / sizeof(mmu_descriptor_long_t))
#define MMU_TABLE_ADDR_BITS_LONG	(PAGE_ADDR_BITS - 3)
#define MMU_TABLE_LEVELS_LONG		((32 - PAGE_ADDR_BITS) / MMU_TABLE_ADDR_BITS_LONG)
#define MMU_TABLE_INITIAL_SHIFT_LONG	((32 - PAGE_ADDR_BITS) % MMU_TABLE_ADDR_BITS_SHORT)

#define MMU_TABLE_ADDRESS_LONG(desc)			((uint32_t) (((uint32_t) (desc)) & ~(PAGE_SIZE - 1)))
#define MMU_TABLE_STATUS_LONG(desc)			(((uint32_t) (((uint32_t) (desc)) >> 32)) & 0xffff)
#define MMU_TABLE_DESCRIPTOR_LONG(address, status)	((uint64_t) ((((uint64_t) (status)) << 32) | ((uint64_t) (address))))

typedef uint64_t mmu_descriptor_long_t;
typedef mmu_descriptor_long_t mmu_table_long_t[MMU_TABLE_SIZE_LONG];


//// Selected Table Size ////

#define MMU_TABLE_SIZE				MMU_TABLE_SIZE_SHORT
#define MMU_TABLE_ADDR_BITS			MMU_TABLE_ADDR_BITS_SHORT
#define MMU_TABLE_LEVELS			MMU_TABLE_LEVELS_SHORT
#define MMU_TABLE_INITIAL_SHIFT			MMU_TABLE_INITIAL_SHIFT_SHORT
#define MMU_DT_TABLE				MMU_DT_TABLE_SHORT
#define MMU_TABLE_ADDRESS(desc)			MMU_TABLE_ADDRESS_SHORT(desc)
#define MMU_TABLE_STATUS(desc)			MMU_TABLE_STATUS_SHORT(desc)
#define MMU_TABLE_DESCRIPTOR(address, status)	MMU_TABLE_DESCRIPTOR_SHORT(address, status)

typedef mmu_descriptor_short_t mmu_descriptor_t;
typedef mmu_table_short_t mmu_table_t;


//// Public Interface ////

#define MMU_FLAG_TYPE		0x07
#define MMU_FLAG_UNMAP		0x00	/// Unmap and free all entries visited
#define MMU_FLAG_UNALLOCATED	0x01	/// Map all table entries, but leave the descriptors invalid, so a page fault can be caught
#define MMU_FLAG_PREALLOCATED	0x02	/// Allocate pages for the entire range
#define MMU_FLAG_DIRECT		0x03	/// Map the entire range to the same physical address as the corresponding virtual address
#define MMU_FLAG_EXISTING	0x04	/// Map the entire range to the given physical address

#define MMU_FLAG_NOCACHE	0x40

#define MMU_FLAG_PRIVATE	0x00
#define MMU_FLAG_SHARED		0x80

int init_mmu(void);
int init_mmu_table(mmu_descriptor_t *root);
int mmu_table_map(mmu_descriptor_t *root, void *address, ssize_t length, int flags);
int mmu_table_unmap(mmu_descriptor_t *root, void *address, ssize_t length);

#endif

