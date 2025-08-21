
#include <errno.h>
#include <string.h>

#include <asm/mmu.h>
#include <kernel/mm/pages.h>
#include <kernel/printk.h>


#define MMU_MOVE_TO_TCR(value)		\
	asm volatile("pmove	%0, %%tc\n" : : "m" (value))

#define MMU_MOVE_TO_CRP(value)		\
	asm volatile("pmove	%0, %%crp\n" : : "m" (value))

#define MMU_MOVE_TO_SRP(value)		\
	asm volatile("pmove	%0, %%srp\n" : : "m" (value))


mmu_descriptor_t *root = NULL;

static inline int init_mmu_table(mmu_descriptor_t *root)
{
	memset(root, '\0', sizeof(mmu_table_t));
	return 0;
}


int init_mmu(void)
{
	uint32_t tcr = 0;
	struct mmu_root_pointer root_pointer;

	root = mmu_table_alloc();

	// Initialize the top-level kernel page table to map directly to real memory
	if (mmu_table_map(root, (uintptr_t) 0x00000000, 0x01000000, MMU_FLAG_WINDOW) < 0) {
		log_error("error mapping lower memory\n");
	}
	if (mmu_table_map(root, (uintptr_t) 0xFF000000, 0x01000000, MMU_FLAG_WINDOW | MMU_FLAG_NOCACHE) < 0) {
		log_error("error mapping upper memory\n");
	}

	root_pointer.limit = 0;
	// TODO disabled supervisor flag for the time being, until user processes use their own address space
	root_pointer.status = MMU_STATUS_DEFAULT | /* MMU_STATUS_DESC_S | */ MMU_DT_TABLE;
	root_pointer.table = (uint32_t) root;
	MMU_MOVE_TO_SRP(root_pointer);
	MMU_MOVE_TO_CRP(root_pointer);

	for (short i = 0; i < MMU_TABLE_LEVELS; i++) {
		tcr = (tcr << 4) | MMU_TABLE_ADDR_BITS;
	}

	tcr =	tcr |
		MMU_TC_ENABLE |
		MMU_TC_SRE |
		(PAGE_ADDR_BITS << MMU_TC_PAGE_SIZE_SHIFT) |
		(MMU_TABLE_INITIAL_SHIFT << MMU_TC_INITIAL_SHIFT);

	log_debug("mmu: setting TCR to %x\n", tcr);

	MMU_MOVE_TO_TCR(tcr);
	return 0;
}

mmu_descriptor_t *mmu_table_alloc(void)
{
	mmu_descriptor_t *root;

	root = (mmu_descriptor_t *) page_alloc_single();
	init_mmu_table(root);

	return root;
}

void mmu_table_free(mmu_descriptor_t *root)
{
	// TODO unmap all
	page_free_single((page_t *) root);
}

/// Modify the MMU tables according to the flags
///
/// A memory area starting at `virtual_addr` that is `length` in size will either be added or
/// removed from the MMU table tree that starts at `root`.
///
/// NOTE: The virtual address and length are assumed to be rounded to the nearest PAGE_SIZE
/// and the length is not past the end of the valid address space (no overflow).  They must
/// be validated before calling this function
int mmu_table_map(mmu_descriptor_t *root, uintptr_t virtual_addr, ssize_t length, int flags)
{
	int i;
	int level = 0;
	uint16_t status;
	mmu_descriptor_t *table[5];
	physical_address_t physical_addr;
	uint8_t bits = 32 - MMU_TABLE_INITIAL_SHIFT - MMU_TABLE_ADDR_BITS;

	// NOTE: the virtual_addr and length are assumed to be correct.  The checks are only performed
	// once in memory_map_mmap() before the map is altered

	//printk("mapping %x length %x flags %x\n", virtual_addr, length, flags);
	table[0] = root;
	while (length > 0) {
		i = ((virtual_address_t) virtual_addr >> bits) & ((1 << MMU_TABLE_ADDR_BITS) - 1);
		//printk("bits %d, i %d, table: %x\n", bits, i, table[level]);
		if (bits <= PAGE_ADDR_BITS || length >= (1 << bits)) {
			status = MMU_DT_PAGE_DESCRIPTOR;
			if (flags & MMU_FLAG_NOCACHE) {
				status |= MMU_STATUS_DESC_CI;
			}

			// If there's an existing page allocated, and we're not unmapping, then raise an error
			if (MMU_TABLE_ADDRESS(table[level][i]) != 0) {
				if ((flags & MMU_FLAG_TYPE) != MMU_FLAG_UNMAP) {
					return EEXIST;
				}
			}

			// Determine which address to map to
			switch (flags & MMU_FLAG_TYPE) {
				case MMU_FLAG_UNMAP: {
					if (!(flags & MMU_FLAG_PAGE_BACKED)) {
						// TODO this was causing the crashing problems, because a page was being freed and reused causing corruption (on a user stack I think)
						page_free_single((page_t *) MMU_TABLE_ADDRESS(table[level][i]));
						printk("problematic free page %x\n", MMU_TABLE_ADDRESS(table[level][i]));
					}
					status = 0;
					physical_addr = 0;
					break;
				}
				case MMU_FLAG_UNALLOCATED: {
					physical_addr = 0;
					break;
				}
				case MMU_FLAG_WINDOW: {
					physical_addr = ((physical_address_t) virtual_addr);
					break;
				}
				case MMU_FLAG_PREALLOCATED: {
					physical_addr = (physical_address_t) page_alloc_single();
					break;
				}
				default: {
					return EINVAL;
				}
			}

			// Set the descriptor in the current table
			table[level][i] = MMU_TABLE_DESCRIPTOR(physical_addr, status);
			//printk("set desc %x\n", table[level][i]);

			// Advance to the next address
			length -= 1 << bits;
			virtual_addr = (((virtual_address_t) virtual_addr) + (1 << bits));

			// If we've reached the end of the current table, then ascend one level
			if (i + 1 >= MMU_TABLE_SIZE) {
				level -= 1;
				bits += MMU_TABLE_ADDR_BITS;
			}
		} else {
			// Look up the table containing the current virtual address
			table[level + 1] = (mmu_descriptor_t *) MMU_TABLE_ADDRESS(table[level][i]);
			if (!table[level + 1]) {
				// Create a new table since one hasn't already been created yet
				table[level + 1] = (mmu_descriptor_t *) page_alloc_single();
				init_mmu_table(table[level + 1]);

				// Set the previous level's table with the address of the newly allocated page
				table[level][i] = (physical_address_t) MMU_TABLE_DESCRIPTOR(table[level + 1], MMU_DT_TABLE);
			}
			//printk("level %d, i %d, table %x, length %d\n", level, i, table[level][i], length);

			// Decend one level into the next table
			level += 1;
			bits -= MMU_TABLE_ADDR_BITS;
		}
	}
	return 0;
}

