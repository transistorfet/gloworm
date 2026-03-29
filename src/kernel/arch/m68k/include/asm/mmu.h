
#ifndef _ARCH_M68K_ASM_MMU_H
#define _ARCH_M68K_ASM_MMU_H

#include <stddef.h>
#include <stdint.h>

#include <asm/addresses.h>
#include <kernel/mm/pages.h>


#define MMU_MOVE_TO_TCR(value)		\
	asm volatile("pmove	%0, %%tc\n" : : "m" (value))

#define MMU_MOVE_FROM_TCR(value)	\
	asm volatile("pmove	%%tc, %0\n" : "=m" (value))

#define MMU_MOVE_TO_SRP(value)		\
	asm volatile("pmove	%0, %%srp\n" : : "m" (value))

#define MMU_MOVE_FROM_SRP(value)	\
	asm volatile("pmove	%%srp, %0\n" : "=m" (value))

#define MMU_MOVE_TO_CRP(value)		\
	asm volatile("pmove	%0, %%crp\n" : : "m" (value))

#define MMU_MOVE_FROM_CRP(value)	\
	asm volatile("pmove	%%crp, %0\n" : "=m" (value))

#define MMU_MOVE_TO_TT0(value)		\
	asm volatile("pmove	%0, %%tt0\n" : : "m" (value))

#define MMU_MOVE_TO_TT1(value)		\
	asm volatile("pmove	%0, %%tt1\n" : : "m" (value))

#define MMU_FLUSH_ALL()		\
	asm volatile("pflusha\n")

#define MMU_FLUSH_USER_TLB()		\
	asm volatile("pflush	#4, #1\n")

#define MMU_FLUSH_ONE(addr)		\
	asm volatile("pflush	#4, #1, %0\n" : : "m" (addr))


#define MMU_DT_INVALID		0
#define MMU_DT_PAGE_DESCRIPTOR	1
#define MMU_DT_TABLE_SHORT	2
#define MMU_DT_TABLE_LONG	3

#define MMU_STATUS_DESC_TYPE	0x0003
#define MMU_STATUS_DESC_WP	0x0004
#define MMU_STATUS_DESC_U	0x0008
#define MMU_STATUS_DESC_M	0x0010
#define MMU_STATUS_DESC_CI	0x0040
#define MMU_STATUS_DESC_S_LONG	0x0100
#define MMU_STATUS_DEFAULT_LONG	0xFC00

#define MMU_TC_ENABLE		0x80000000
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

#define MMU_DT(desc)		(MMU_TABLE_STATUS(desc) & MMU_STATUS_DESC_TYPE)

#define MMU_FLAG_TYPE		0x0007
#define MMU_FLAG_UNMAP		0x0001	/// Unmap and free all entries visited
#define MMU_FLAG_UNALLOCATED	0x0002	/// Map all table entries, but leave the descriptors invalid, so a page fault can be caught
#define MMU_FLAG_PREALLOCATED	0x0003	/// Allocate pages for the entire range
#define MMU_FLAG_WINDOW		0x0004	/// Map the entire range to the same physical address as the corresponding virtual address
#define MMU_FLAG_MODIFY		0x0005	/// Modify an existing mapping

#define MMU_FLAG_SUPERVISOR	0x0010	/// Set the supervisor flag on mapped pages
#define MMU_FLAG_NOCACHE	0x0040	/// Disable hardware caching for the mapped range (eg. for memory-mapped device access)
#define MMU_FLAG_COPY_ON_WRITE	0x0080	/// Force the page descriptors to be read-only so that a write will cause a page fault

#define MMU_FLAG_WRITE		0x1000	/// Enable writing to the mapped area
#define MMU_FLAG_EXEC		0x0000	/// Enable code execution (not supported by 68k, but included for compatibility with other archs)

//#define MMU_FLAG_PRIVATE	0x00
//#define MMU_FLAG_SHARED	0x80

int init_mmu(mmu_descriptor_t *supervisor_table);
mmu_descriptor_t *mmu_table_alloc(void);
void mmu_table_free(mmu_descriptor_t *root);
int mmu_table_map(mmu_descriptor_t *root, uintptr_t address, ssize_t length, int flags);
int mmu_table_copy(mmu_descriptor_t *dest_table, mmu_descriptor_t *src_table, uintptr_t virtual_addr, ssize_t length, int flags);
physical_address_t mmu_table_get_page(mmu_descriptor_t *root_table, uintptr_t virtual_addr, size_t *page_size);
int mmu_table_set_page(mmu_descriptor_t *root_table, uintptr_t virtual_addr, uintptr_t physical_addr, size_t page_size, int flags);
int mmu_table_validate_user_address(mmu_descriptor_t *root_table, uintptr_t virtual_addr);
int mmu_table_print(mmu_descriptor_t *root);

static inline void mmu_table_switch(mmu_descriptor_t *root)
{
	struct mmu_root_pointer root_pointer;

	root_pointer.limit = 0x7fff;
	root_pointer.status = MMU_DT_TABLE;
	root_pointer.table = (uint32_t) root;
	MMU_MOVE_TO_CRP(root_pointer);

	MMU_FLUSH_USER_TLB();
}

#endif

