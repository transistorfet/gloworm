
#ifndef _ARCH_TESTAPP_ASM_MMU_H
#define _ARCH_TESTAPP_ASM_MMU_H

#include <stddef.h>
#include <stdint.h>

#include <asm/addresses.h>
#include <kernel/mm/pages.h>


typedef uintptr_t mmu_table_t;
typedef uintptr_t mmu_descriptor_t;

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

int init_mmu(void);
mmu_descriptor_t *mmu_table_alloc(void);
void mmu_table_free(mmu_descriptor_t *root);
int mmu_table_map(mmu_descriptor_t *root, uintptr_t address, ssize_t length, int flags);
int mmu_table_copy(mmu_descriptor_t *dest_table, mmu_descriptor_t *src_table, uintptr_t virtual_addr, ssize_t length, int flags);
page_t *mmu_table_get_page(mmu_descriptor_t *root_table, uintptr_t virtual_addr);
int mmu_table_set_page(mmu_descriptor_t *root_table, uintptr_t virtual_addr, uintptr_t physical_addr, int flags);
int mmu_table_validate_user_address(mmu_descriptor_t *root_table, uintptr_t virtual_addr);
int mmu_table_print(mmu_descriptor_t *root);

/*
static inline void mmu_table_switch(mmu_descriptor_t *root)
{
	struct mmu_root_pointer root_pointer;

	root_pointer.limit = 0x7fff;
	root_pointer.status = MMU_DT_TABLE;
	root_pointer.table = (uint32_t) root;
	MMU_MOVE_TO_CRP(root_pointer);

	MMU_FLUSH_USER_TLB();
}
*/

#endif

