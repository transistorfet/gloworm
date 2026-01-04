
#include <errno.h>
#include <string.h>

#include <asm/mmu.h>
#include <kernel/mm/pages.h>
#include <kernel/printk.h>


#define TABLE_INDEX(vaddr, bits) \
	( ((vaddr) >> (bits)) & ((1 << MMU_TABLE_ADDR_BITS) - 1) )


mmu_descriptor_t *kernel_mmu_table = NULL;

static inline int init_mmu_table(mmu_descriptor_t *root_table)
{
	memset(root_table, '\0', sizeof(mmu_table_t));
	return 0;
}


int init_mmu(void)
{
	uint32_t tt = 0;
	uint32_t tcr = 0;
	struct mmu_root_pointer root_pointer;

	kernel_mmu_table = mmu_table_alloc();

	// Initialize the top-level kernel page table to map directly to real memory
	if (mmu_table_map(kernel_mmu_table, (uintptr_t) 0x00000000, 0x01000000, MMU_FLAG_WINDOW | MMU_FLAG_SUPERVISOR | MMU_FLAG_WRITE) < 0) {
		log_error("error mapping lower memory\n");
	}
	if (mmu_table_map(kernel_mmu_table, (uintptr_t) 0xFF000000, 0x01000000, MMU_FLAG_WINDOW | MMU_FLAG_NOCACHE | MMU_FLAG_SUPERVISOR | MMU_FLAG_WRITE) < 0) {
		log_error("error mapping upper memory\n");
	}

	MMU_MOVE_TO_TT0(tt);
	MMU_MOVE_TO_TT1(tt);

	root_pointer.limit = 0x7fff;
	root_pointer.status = MMU_DT_TABLE;
	root_pointer.table = (uint32_t) kernel_mmu_table;
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

	tcr = 0;
	MMU_MOVE_FROM_TCR(tcr);
	log_debug("mmu: read back TCR of %x\n", tcr);
	return 0;
}

mmu_descriptor_t *mmu_table_alloc(void)
{
	mmu_descriptor_t *root_table;

	root_table = (mmu_descriptor_t *) page_alloc_single();
	init_mmu_table(root_table);

	return root_table;
}

void mmu_table_free_inner(mmu_descriptor_t *table, uint8_t bits)
{
	for (int i = 0; i < MMU_TABLE_SIZE; i++) {
		switch (MMU_DT(table[i])) {
			case MMU_DT_PAGE_DESCRIPTOR:
				// TODO how do you know if it's a WINDOW or an actual page?
				if (bits == PAGE_ADDR_BITS) {
					page_free_single((page_t *) MMU_TABLE_ADDRESS(table[i]));
				} else {
					page_free_contiguous((page_t *) MMU_TABLE_ADDRESS(table[i]), 1 << bits);
				}
				break;
			case MMU_DT_TABLE_SHORT:
			case MMU_DT_TABLE_LONG:
				mmu_table_free_inner((mmu_descriptor_t *) MMU_TABLE_ADDRESS(table[i]), bits - MMU_TABLE_ADDR_BITS);
				break;
			default:
				break;
		}
	}
	page_free_single((page_t *) table);
}

void mmu_table_free(mmu_descriptor_t *root_table)
{
	uint8_t bits = 32 - MMU_TABLE_INITIAL_SHIFT - MMU_TABLE_ADDR_BITS;
	mmu_table_free_inner(root_table, bits);
}


#define CREATE_IF_NEEDED		1

struct get_table_result {
	mmu_descriptor_t *table;
	uint8_t bits;
};

static inline int get_table(mmu_descriptor_t *root_table, virtual_address_t virtual_addr, ssize_t length, int create_if_needed, struct get_table_result *result)
{
	int i;
	char request_covers_whole_chunk;
	mmu_descriptor_t *table, *next_table;
	uint8_t bits = 32 - MMU_TABLE_INITIAL_SHIFT - MMU_TABLE_ADDR_BITS;

	table = root_table;
	while (1) {
		i = TABLE_INDEX(virtual_addr, bits);
		//printk("%08x: bits %d, i %d, table: %x\n", virtual_addr, bits, i, table);

		request_covers_whole_chunk = (virtual_addr & ((1 << bits) - 1)) == 0 && length >= (1 << bits);

		if (MMU_DT(table[i]) == MMU_DT_PAGE_DESCRIPTOR) {
			result->bits = bits;
			if (!request_covers_whole_chunk) {
				// There's an early termination page descriptor entry, but we're only mapping a virtual address that's within this single page
				// instead of mapping the whole page, which is an error
				result->table = NULL;
				return EEXIST;
			} else {
				result->table = table;
				return 0;
			}
		} else if (bits <= PAGE_ADDR_BITS || request_covers_whole_chunk) {
			result->table = table;
			result->bits = bits;
			return 0;
		} else {
			if (MMU_DT(table[i]) == MMU_DT_INVALID) {
				if (create_if_needed) {
					// Create a new table since one hasn't already been created yet
					next_table = (mmu_descriptor_t *) page_alloc_single();
					init_mmu_table(next_table);

					//printk("set new table %x [table: %x, i: %d, bits: %d]\n", next_table, table, i, bits);
					// Set the previous level's table with the address of the newly allocated page
					table[i] = (physical_address_t) MMU_TABLE_DESCRIPTOR(next_table, MMU_DT_TABLE);
				} else {
					result->table = NULL;
					result->bits = bits;
					return ENOENT;
				}
			}
			//printk("descending into %x [table: %x, i: %d, bits: %d]\n", next_table, table, i, bits);
			table = (mmu_descriptor_t *) MMU_TABLE_ADDRESS(table[i]);

			// Decend one level into the next table
			bits -= MMU_TABLE_ADDR_BITS;
		}
	}
	result->table = NULL;
	result->bits = 0;
	return ENOENT;
}

/// Modify the MMU tables according to the flags
///
/// A memory area starting at `virtual_addr` that is `length` in size will either be added or
/// removed from the MMU table tree that starts at `root_table`.
///
/// NOTE: The virtual address and length are assumed to be rounded to the nearest PAGE_SIZE
/// and the length is not past the end of the valid address space (no overflow).  They must
/// be validated before calling this function
int mmu_table_map(mmu_descriptor_t *root_table, uintptr_t virtual_addr, ssize_t length, int flags)
{
	int i;
	int error;
	uint16_t status;
	struct get_table_result result;
	physical_address_t physical_addr;

	// NOTE: the virtual_addr and length are assumed to be correct.  The checks are only performed
	// once in memory_map_mmap() before the map is altered

	// Force a table update in the first iteration
	i = MMU_TABLE_SIZE;

	for (; length > 0; i++) {
		// If we've reached the end of the current table, then ascend one level
		if (i + 1 >= MMU_TABLE_SIZE) {
			error = get_table(root_table, virtual_addr, length, CREATE_IF_NEEDED, &result);
			if (error < 0) {
				return error;
			}
		}

		i = TABLE_INDEX(virtual_addr, result.bits);

		status = 0;
		if (flags & MMU_FLAG_NOCACHE) {
			status |= MMU_STATUS_DESC_CI;
		}
		//if (flags & MMU_FLAG_SUPERVISOR) {
		//	status |= MMU_STATUS_DESC_S;
		//}
		if (!(flags & MMU_FLAG_WRITE)) {
			status |= MMU_STATUS_DESC_WP;
		}

		// If there's an existing page allocated, and we're not unmapping, then raise an error
		if (MMU_TABLE_ADDRESS(result.table[i]) != 0) {
			if ((flags & MMU_FLAG_TYPE) != MMU_FLAG_UNMAP) {
				return EEXIST;
			}
		}

		// Determine which address to map to
		switch (flags & MMU_FLAG_TYPE) {
			case MMU_FLAG_UNMAP: {
				if (!(flags & MMU_FLAG_WINDOW)) {
					// TODO this was causing the crashing problems, because a page was being freed and reused causing corruption (on a user stack I think)
					page_free_single((page_t *) MMU_TABLE_ADDRESS(result.table[i]));
					printk("problematic free page %x\n", MMU_TABLE_ADDRESS(result.table[i]));
				}
				physical_addr = 0;
				break;
			}
			case MMU_FLAG_UNALLOCATED: {
				physical_addr = 0;
				break;
			}
			case MMU_FLAG_WINDOW: {
				physical_addr = ((physical_address_t) virtual_addr);
				status |= MMU_DT_PAGE_DESCRIPTOR;
				break;
			}
			case MMU_FLAG_PREALLOCATED: {
				physical_addr = (physical_address_t) page_alloc_single();
				status |= MMU_DT_PAGE_DESCRIPTOR;
				break;
			}
			case MMU_FLAG_MODIFY: {
				// Use the existing address unmodified
				physical_addr = MMU_TABLE_ADDRESS(result.table[i]);
				// Preserve the existing descriptor type, but otherwise overwrite the flags
				status |= MMU_DT(result.table[i]);
				break;
			}
			default: {
				return EINVAL;
			}
		}

		// Set the descriptor in the current table
		result.table[i] = MMU_TABLE_DESCRIPTOR(physical_addr, status);
		//printk("%08x: %x (%x) [table: %x, i: %d]\n", virtual_addr, MMU_TABLE_ADDRESS(result.table[i]), MMU_TABLE_STATUS(result.table[i]), result.table, i);

		// Advance to the next address
		length -= 1 << result.bits;
		virtual_addr = (((virtual_address_t) virtual_addr) + (1 << result.bits));
	}

	return 0;
}

/// Copy a range of an MMU table into another
///
/// A memory area starting at `virtual_addr` that is `length` in size will be copied, and
/// page refcounts incremented, from `src_table` to `dest_table`.  The virtual address must
/// be the same in both tables.
///
/// NOTE: The virtual address and length are assumed to be rounded to the nearest PAGE_SIZE
/// and the length is not past the end of the valid address space (no overflow).  They must
/// be validated before calling this function
int mmu_table_copy(mmu_descriptor_t *dest_table, mmu_descriptor_t *src_table, uintptr_t virtual_addr, ssize_t length, int flags)
{
	int i;
	int error;
	int additional_status = 0;
	struct get_table_result src_result, dest_result;

	// NOTE: the virtual_addr and length are assumed to be correct.  The checks are only performed
	// once in memory_map_mmap() before the map is altered

	if (flags & MMU_FLAG_COPY_ON_WRITE) {
		additional_status |= MMU_STATUS_DESC_WP;
	}

	// Force a table update in the first iteration
	i = MMU_TABLE_SIZE;

	for (; length > 0; i++) {
		// If we've reached the end of the current table, then ascend one level
		if (i + 1 >= MMU_TABLE_SIZE) {
			error = get_table(src_table, virtual_addr, length, 0, &src_result);
			if (error < 0) {
				return error;
			}

			error = get_table(dest_table, virtual_addr, length, CREATE_IF_NEEDED, &dest_result);
			if (error < 0) {
				return error;
			}

			if (src_result.bits != dest_result.bits) {
				return EFAULT;
			}
		}

		i = TABLE_INDEX(virtual_addr, src_result.bits);

		// Set the descriptor in the current table, and force the write protect flag on, so we'll
		// get an exception when trying to write to the new page
		dest_result.table[i] = src_result.table[i] | additional_status;
		src_result.table[i] |= additional_status;
		if (MMU_TABLE_ADDRESS(src_result.table[i]) != 0) {
			if (src_result.bits == PAGE_ADDR_BITS) {
				page_make_ref_single((page_t *) MMU_TABLE_ADDRESS(src_result.table[i]));
			} else {
				page_make_ref_contiguous((page_t *) MMU_TABLE_ADDRESS(src_result.table[i]), 1 << src_result.bits);
			}
		}
		//printk("%08x: %x (%x) [table: %x, i: %d]\n", virtual_addr, MMU_TABLE_ADDRESS(dest_result.table[i]), MMU_TABLE_STATUS(dest_result.table[i]), dest_result.table, i);

		// Advance to the next address
		length -= 1 << src_result.bits;
		virtual_addr = (((virtual_address_t) virtual_addr) + (1 << src_result.bits));
	}

	return 0;
}

page_t *mmu_table_get_page(mmu_descriptor_t *root_table, uintptr_t virtual_addr)
{
	int error;
	struct get_table_result result;

	error = get_table(root_table, virtual_addr, PAGE_SIZE, 0, &result);
	if (error < 0) {
		return NULL;
	}

	return (page_t *) MMU_TABLE_ADDRESS(result.table[TABLE_INDEX(virtual_addr, result.bits)]);
}

int mmu_table_set_page(mmu_descriptor_t *root_table, uintptr_t virtual_addr, uintptr_t physical_addr, int flags)
{
	int i;
	int error;
	int status;
	struct get_table_result result;

	error = get_table(root_table, virtual_addr, PAGE_SIZE, CREATE_IF_NEEDED, &result);
	if (error < 0) {
		return error;
	}

	if (result.bits != PAGE_ADDR_BITS) {
		printk("ERROR: there's something odd in mmu_table_set_page(), I expected only a single page, but got a higher level granule\n");
		return EFAULT;
	}

	i = TABLE_INDEX(virtual_addr, result.bits);

	status = MMU_TABLE_STATUS(result.table[i]) | MMU_DT_PAGE_DESCRIPTOR;
	if (flags & MMU_FLAG_WRITE) {
		status &= ~MMU_STATUS_DESC_WP;
	} else {
		status |= MMU_STATUS_DESC_WP;
	}

	result.table[i] = MMU_TABLE_DESCRIPTOR(physical_addr, status);
	return 0;
}

static int mmu_table_print_inner(mmu_descriptor_t *table, uint8_t bits, uintptr_t virtual_addr)
{
	int i;

	printk("mmu table for addresses %x to %x [table: %x, bits: %d]\n", virtual_addr, virtual_addr + (1 << bits) - 1, table, bits);
	for (i = 0; i < MMU_TABLE_SIZE; i++) {
		if ((table[i] & MMU_STATUS_DESC_TYPE) == MMU_DT_TABLE) {
			printk("decending at [table: %x, i: %d, bits: %d] into table %x (%x)\n", table, i, bits, MMU_TABLE_ADDRESS(table[i]), MMU_TABLE_STATUS(table[i]));
			mmu_table_print_inner((mmu_descriptor_t *) MMU_TABLE_ADDRESS(table[i]), bits - MMU_TABLE_ADDR_BITS, virtual_addr);
		} else {
			if (table[i]) {
				printk("%x: %x to %x (%x) [table: %x, i: %d, bits: %d]\n", virtual_addr, MMU_TABLE_ADDRESS(table[i]), MMU_TABLE_ADDRESS(table[i]) + (1 << bits) - 1, MMU_TABLE_STATUS(table[i]), table, i, bits);
			}
		}
		virtual_addr += (1 << bits);
	}
	return 0;
}

/// Print the MMU table
///
/// NOTE: this uses the log interface to output the table
int mmu_table_print(mmu_descriptor_t *root_table)
{
	return mmu_table_print_inner(root_table, 32 - MMU_TABLE_INITIAL_SHIFT - MMU_TABLE_ADDR_BITS, 0);
}

