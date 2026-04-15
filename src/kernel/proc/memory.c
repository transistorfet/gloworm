
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <kconfig.h>
#include <asm/types.h>
#include <sys/param.h>

#include <kernel/printk.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/mm/pages.h>
#include <kernel/fs/fileptr.h>
#include <kernel/proc/memory.h>
#include <kernel/arch/context.h>
#include <kernel/proc/process.h>
#include <kernel/utils/math.h>
#include <kernel/utils/iovec.h>


#if !defined(CONFIG_MMU)
struct memory_region *memory_region_alloc_user_memory(size_t size, struct vfile *file)
{
	char *memory = NULL;
	struct memory_region *region;

	// Round the size up to the nearest page
	size = roundup(size, PAGE_SIZE);

	region = kzalloc(sizeof(struct memory_region));
	if (!region)
		goto fail;

	memory = (char *) page_alloc_contiguous(size);
	if (!memory)
		goto fail;

	region->refcount = 1;
	region->file = file;
	region->mem_start = memory;
	region->mem_length = size;

	return region;

fail:
	if (file)
		vfs_close(file);
	if (memory)
		kmfree(memory);
	return NULL;
}

void memory_region_free(struct memory_region *region)
{
	if (!region)
		return;

	if (--region->refcount == 0) {
		page_free_contiguous((uintptr_t) region->mem_start, region->mem_length);
		kmfree(region);
	}
}
#endif


#if defined(CONFIG_MMU)
physical_address_t file_memory_ops_load_page_at(struct memory_segment *segment, virtual_address_t vaddr)
{
	int error;
	struct kvec kvec;
	offset_t file_offset;
	physical_address_t page;
	struct iovec_iter iter;

	page = page_alloc_single();
	if (!page) {
		return NULL;
	}
	zero_page(page);

	file_offset = rounddown(vaddr - segment->start, PAGE_SIZE) + rounddown(segment->offset, PAGE_SIZE);
	//printk("file offset: %x vaddr %x start %x end %x offset into segment %x, offset %x\n", file_offset, vaddr, segment->start, segment->end, (vaddr - segment->start), segment->offset);
	error = vfs_seek(segment->file, file_offset, SEEK_SET);
	if (error < 0) {
		return NULL;
	}

	iovec_iter_init_simple_kvec(&iter, &kvec, (char *) page, PAGE_SIZE);
	error = vfs_read(segment->file, &iter);
	if (error < 0) {
		return NULL;
	}

	//printk("newly loaded page, %d bytes\n", error);
	//printk_dump((uint16_t *) page, PAGE_SIZE);

	return page;
}

struct memory_ops file_memory_ops = {
	.load_page_at	= file_memory_ops_load_page_at,
};


physical_address_t anonymous_memory_ops_load_page_at(struct memory_segment *segment, virtual_address_t vaddr)
{
	physical_address_t page;

	page = page_alloc_single();
	if (!page) {
		return NULL;
	}
	zero_page(page);
	return page;
}

struct memory_ops anonymous_memory_ops = {
	.load_page_at	= anonymous_memory_ops_load_page_at,
};

static inline int memory_map_convert_copy_on_write(struct memory_map *map, virtual_address_t page_address, physical_address_t existing_page, size_t page_size)
{
	int error;
	physical_address_t page_copy;

	if (page_size == PAGE_SIZE) {
		page_copy = page_alloc_single();
	} else {
		page_copy = page_alloc_contiguous(page_size);
	}
	if (!page_copy) {
		return ENOMEM;
	}
	memcpy((void *) page_copy, (void *) existing_page, page_size);

	//log_debug("copying page for %x (originally %x) to %x\n", page_address, existing_page, page_copy);
	error = mmu_table_set_page(map->root_table, page_address, (uintptr_t) page_copy, page_size, MMU_FLAG_WRITE);
	if (error < 0) {
		return error;
	}

	if (page_size == PAGE_SIZE) {
		page_free_single((physical_address_t) existing_page);
	} else {
		page_free_contiguous((physical_address_t) existing_page, page_size);
	}
	return 0;
}

// TODO rename to something inclusive of CoW
int memory_map_load_page_at(struct memory_map *map, virtual_address_t vaddr, uint16_t write_error)
{
	int error;
	uintptr_t page_address;
	physical_address_t new_page;
	struct memory_segment *segment;
	struct get_page_result result;

	segment = memory_map_find(map, vaddr);
	if (!segment) {
		return ENOENT;
	}

	if (segment->ops && segment->ops->load_page_at) {
		page_address = rounddown(vaddr, PAGE_SIZE);
		// TODO should you add support for large pages?
		error = mmu_table_get_page(map->root_table, page_address, &result);
		if (!error) {
			if (write_error && (segment->flags & SEG_WRITE)) {
				return memory_map_convert_copy_on_write(map, page_address, result.phys, result.size);
			} else {
				log_error("attempting to read in data space a page that already exists\n");
				return EEXIST;
			}
		} else {
			if (write_error && !(segment->flags & SEG_WRITE)) {
				log_error("attempting to write to a read-only segment %x to %x\n", segment->start, segment->end);
				return EPERM;
			}
			new_page = segment->ops->load_page_at(segment, vaddr);
			if (!new_page) {
				return ENOMEM;
			}
			//log_debug("loading new page for %x -> %x\n", vaddr, new_page);
			error = mmu_table_set_page(map->root_table, page_address, (uintptr_t) new_page, PAGE_SIZE, (segment->flags & SEG_WRITE) ? MMU_FLAG_WRITE : 0);
			if (error < 0) {
				log_error("error setting newly loaded page: %d\n", error);
				page_free_single((physical_address_t) new_page);
				return error;
			}
		}
		return 0;
	} else {
		return ENOENT;
	}
}

int memory_map_load_pages_into_kvec(struct memory_map *map, struct kvec *kvec, int max_segs, virtual_address_t start, size_t length, int write_flag)
{
	int i;
	int error;
	size_t page_offset, page_remaining;
	struct get_page_result page;
	virtual_address_t page_start;

	page_start = rounddown(start, PAGE_SIZE);
	for (i = 0; i < max_segs; i++) {
		error = mmu_table_get_page(map->root_table, page_start, &page);
		if (error < 0) {
			error = memory_map_load_page_at(map, page_start, write_flag);
			if (error < 0) {
				return error;
			}

			error = mmu_table_get_page(map->root_table, page_start, &page);
			if (error < 0) {
				return ENOENT;
			}
		}

		page_offset = alignment_offset(start, page.size);
		page_remaining = page.size - page_offset;
		kvec[i].buf = (char *) page.phys + page_offset;
		if (length > page_remaining) {
			kvec[i].bytes = page_remaining;
			length -= page_remaining;
		} else {
			kvec[i].bytes = length;
			return i + 1;
		}
		page_start = page_start + page.size;
		// Reset the start address so that the page_offset will be 0 except for the first iteration
		start = 0;
	}
	// If we ran out of kvec entries before we reached the length, then return an error
	return ENOMEM;
}
#endif

struct memory_segment *memory_segment_alloc(uintptr_t start, uintptr_t end, int flags)
{
	struct memory_segment *segment;

	segment = kmalloc(sizeof(struct memory_segment));
	if (!segment) {
		return NULL;
	}

	_queue_node_init(&segment->node);
	segment->flags = flags;
	segment->start = start;
	segment->end = end;
	#if defined(CONFIG_MMU)
	segment->ops = NULL;
	#else
	segment->region = NULL;
	#endif

	return segment;
}

void memory_segment_free(struct memory_segment *segment)
{
	#if defined(CONFIG_MMU)
	if (segment->file) {
		vfs_close(segment->file);
	}
	#else
	memory_region_free(segment->region);
	#endif
	kmfree(segment);
}

struct memory_map *memory_map_alloc(void)
{
	struct memory_map *map;

	map = kzalloc(sizeof(struct memory_map));
	if (!map) {
		return NULL;
	}

	map->refcount = 1;
	_queue_init(&map->segments);

	#if defined(CONFIG_MMU)
	map->root_table = mmu_table_alloc();
	if (!map->root_table) {
		memory_map_free(map);
		return NULL;
	}
	#endif

	return map;
}

void memory_map_free(struct memory_map *map)
{
	struct memory_segment *cur, *next;

	if (!map) {
		return;
	}

	if (--map->refcount == 0) {
		#if defined(CONFIG_MMU)
		if (map->root_table) {
			mmu_table_free(map->root_table);
		}
		#endif

		for (cur = _queue_head(&map->segments); cur; cur = next) {
			next = _queue_next(&cur->node);
			memory_segment_free(cur);
		}
		kmfree(map);
	}
}

static inline struct memory_segment *memory_map_insert_segment(struct memory_map *map, uintptr_t start, size_t length, int flags)
{
	uintptr_t end;
	struct memory_segment *cur, *next, *segment;

	end = start + length;

	// Find the location to insert the new segment before allocating it
	for (cur = _queue_head(&map->segments); cur; cur = next) {
		next = _queue_next(&cur->node);
		if (cur->end <= start && (!next || end <= next->start)) {
			break;
		}
	}

	segment = memory_segment_alloc(start, end, flags);
	if (!segment) {
		return NULL;
	}

	// If cur is NULL (ie. no space found after first segment), then insert it at the beginning
	_queue_insert_after(&map->segments, &segment->node, cur ? &cur->node : NULL);

	return segment;
}

static inline void memory_map_adjust_markers(struct memory_map *map, struct memory_segment *segment)
{
	// Adjust memory markers, if needed
	if (segment->flags & SEG_EXECUTABLE) {
		if (!map->code_start) {
			map->code_start = segment->start;
		}
	} else {
		if (!map->data_start) {
			map->data_start = segment->start;
		}
	}

	if ((segment->flags & SEG_TYPE_STACK) && segment->end > map->stack_end) {
		map->stack_end = segment->end;
	}
}

/// Map the given address range to the given region
///
/// NOTE: this will not unmap any previously mapped regions nor will it check for
/// existing mappings, so it should be paired with an unmap call when modifying
/// an existing map
int memory_map_mmap(struct memory_map *map, uintptr_t start, size_t length, int flags, MEMORY_OBJECT_T *object, offset_t offset)
{
	int error = 0;
	struct memory_segment *segment;

	// Check that the length is a multiple of the page size, or raise an error
	if (alignment_offset(length, PAGE_SIZE)) {
		return EINVAL;
	}

	// Check that the virtual address given is aligned to a page, and raise an error if it's not
	if (alignment_offset(start, PAGE_SIZE)) {
		return EINVAL;
	}

	// Check if the segment to be mapped will wrap around at the end of the address space, and
	// raise an error if it will
	if (start + length > 0 && start + length < start) {
		return EINVAL;
	}

	segment = memory_map_insert_segment(map, start, length, flags);
	if (!segment) {
		error = ENOMEM;
		goto fail;
	}

	#if defined(CONFIG_MMU)

	if (flags & SEG_FIXED) {
		segment->ops = NULL;
	} else if (object) {
		segment->ops = &file_memory_ops;
	} else {
		segment->ops = &anonymous_memory_ops;
	}
	segment->file = object;
	segment->offset = offset;

	int mmu_flags = 0;
	if (flags & SEG_WRITE) {
		mmu_flags |= MMU_FLAG_WRITE;
	}

	if (flags & SEG_FIXED) {
		mmu_flags |= MMU_FLAG_WINDOW;
	} else if (flags & SEG_ANONYMOUS) {
		mmu_flags |= MMU_FLAG_UNALLOCATED;
	} else if (flags & SEG_POPULATE) {
		mmu_flags |= MMU_FLAG_PREALLOCATED;
	} else {
		mmu_flags |= MMU_FLAG_UNALLOCATED;
	}

	if (flags & SEG_NOCACHE) {
		mmu_flags |= MMU_FLAG_NOCACHE;
	}

	if (flags & SEG_SUPERVISOR) {
		mmu_flags |= MMU_FLAG_SUPERVISOR;
	}

	error = mmu_table_map(map->root_table, start, length, mmu_flags);
	if (error < 0) {
		goto fail;
	}

	#else

	segment->region = object;

	#endif

	memory_map_adjust_markers(map, segment);

	return 0;

fail:
	if (object) {
		MEMORY_OBJECT_FREE(object);
	}
	return error;
}

/// Unmap the given address range
///
/// If a segment is larger than the given range, it will be split in two.  Otherwise
/// it will be deleted or shrunk accordingly to make room.
int memory_map_unmap(struct memory_map *map, uintptr_t start, size_t length)
{
	uintptr_t end;
	struct memory_segment *cur, *next, *new;

	#if defined(CONFIG_MMU)
	int error;
	int sub_start, sub_end;
	#endif

	end = start + length;
	for (cur = memory_map_iter_first(map); cur; cur = next) {
		#if defined(CONFIG_MMU)
		sub_start = sub_end = 0;
		#endif
		next = memory_map_iter_next(cur);
		if (cur->start >= start && cur->start <= end) {
			#if defined(CONFIG_MMU)
			sub_start = cur->start;
			#endif
			if (cur->end >= start && cur->end <= end) {
				// The segment is entirely within the unmap region so delete it entirely
				_queue_remove(&map->segments, &cur->node);
				memory_segment_free(cur);
				#if defined(CONFIG_MMU)
				sub_end = cur->end;
				#endif
			} else {
				// The start will be unmapped but not the end, so move the segment start
				cur->start = end;
				#if defined(CONFIG_MMU)
				sub_end = end;
				#endif
			}
		} else if (cur->end >= start && cur->end <= end) {
			// The end will be unmapped but not the start, so move the segment end
			#if defined(CONFIG_MMU)
			sub_start = start;
			sub_end = cur->end;
			#endif
			cur->end = start;
		} else if (start >= cur->start && start <= cur->end) {
			//  The unmapped region is entirely within this segment, so we need to split it in two
			new = memory_segment_alloc(end, cur->end, cur->flags);
			if (!new) {
				return ENOMEM;
			}
			#if defined(CONFIG_MMU)
			new->ops = cur->ops;
			new->file = MEMORY_OBJECT_MAKE_REF(cur->file);
			sub_start = start;
			sub_end = end;
			#else
			new->region = MEMORY_OBJECT_MAKE_REF(cur->region);
			#endif
			new->offset = cur->offset + (end - cur->start);
			_queue_insert_after(&map->segments, &new->node, &cur->node);

			// Adjust the current segment to end at the start of the region to be unmapped
			cur->end = start;
		}

		#if defined(CONFIG_MMU)
		if (sub_start) {
			error = mmu_table_map(map->root_table, sub_start, sub_end - sub_start, MMU_FLAG_UNMAP | (cur->flags & SEG_FIXED ? MMU_FLAG_WINDOW : 0));
			if (error < 0)
				return error;
		}
		#endif
	}

	// TODO this is wrong, they should find the next lowest valid mapped address to assign
	// it's also not clear what to do when the sbrk or stack_end are unmapped, so maybe
	// making them NULL is the better option, even though that can cause its own issues
	if (map->code_start >= start && map->code_start <= end)
		map->code_start = start;
	if (map->data_start >= start && map->data_start <= end)
		map->data_start = start;
	if (map->heap_start >= start && map->heap_start <= end)
		map->heap_start = start;

	return 0;
}

int memory_map_copy_segment(struct memory_map *dest_map, struct memory_map *src_map, struct memory_segment *src_segment)
{
	struct memory_segment *segment;

	segment = memory_map_insert_segment(dest_map, src_segment->start, src_segment-> end - src_segment->start, src_segment->flags);
	if (!segment) {
		return ENOMEM;
	}

	segment->offset = src_segment->offset;

	#if defined(CONFIG_MMU)

	segment->ops = src_segment->ops;
	segment->file = MEMORY_OBJECT_MAKE_REF(src_segment->file);

	int error;
	int mmu_flags = 0;

	if (src_segment->flags & SEG_WRITE && !(src_segment->flags & SEG_FIXED)) {
		mmu_flags |= MMU_FLAG_COPY_ON_WRITE;
	}

	error = mmu_table_copy(dest_map->root_table, src_map->root_table, src_segment->start, src_segment->end - src_segment->start, mmu_flags);
	if (error < 0)
		return error;

	#else

	segment->region = MEMORY_OBJECT_MAKE_REF(src_segment->region);

	#endif

	memory_map_adjust_markers(dest_map, segment);

	return 0;
}


/// Resize the given segment
///
/// The segment will be resized by `diff` bytes.  If the segment has the MAP_GROWSDOWN
/// flag set, then it will grow the start of the segment downwards.  Otherwise the segment
/// will grow the end of the segment upwards.  If there is an adjacent segment that would
/// overlap after the change, then it is shrunk by the necessary amount
int memory_map_resize(struct memory_segment *segment, ssize_t diff)
{
	uintptr_t new_addr;
	struct memory_segment *adjacent;

	if (segment->flags & SEG_GROWSDOWN) {
		new_addr = segment->start - diff;
		adjacent = _queue_prev(&segment->node);
		if (adjacent && adjacent->end > new_addr) {
			// TODO should this be an error or should it move it?
			if (adjacent->start > new_addr) {
				return EFAULT;
			}
			adjacent->end = new_addr;
		}
		segment->start = new_addr;
	} else {
		new_addr = segment->end + diff;
		adjacent = _queue_next(&segment->node);
		if (adjacent && new_addr > adjacent->start) {
			// TODO should this be an error or should it move it?
			if (new_addr > adjacent->end) {
				return EFAULT;
			}
			adjacent->start = new_addr;
		}
		segment->end = new_addr;
	}
	return 0;
}

int memory_map_move_sbrk(struct memory_map *map, int diff)
{
	int error;
	uintptr_t new_sbrk;
	struct memory_segment *heap, *stack;

	new_sbrk = map->sbrk + diff;

	stack = memory_segment_find_prev(memory_map_iter_last(map), map->sbrk);
	if (!stack) {
		return EFAULT;
	}

	heap = memory_map_iter_prev(stack);
	if (!heap) {
		return EFAULT;
	}

	if (map->sbrk != stack->start) {
		return EFAULT;
	}

	if (((ssize_t) (stack->end - stack->start)) + diff < 0) {
		return ENOMEM;
	}

	// Require an alignment to page size bytes
	if (alignment_offset((diff > 0 ? diff : -diff), PAGE_SIZE)) {
		return ENOMEM;
	}

	if (diff >= 0) {
		error = memory_map_resize(stack, -diff);
	} else {
		error = memory_map_resize(heap, diff);
	}

	if (error < 0) {
		return error;
	}

	if (diff >= 0) {
		error = memory_map_resize(heap, diff);
	} else {
		error = memory_map_resize(stack, -diff);
	}

	if (error < 0) {
		return error;
	}

	map->sbrk = new_sbrk;

	return 0;
}


int memory_map_insert_heap_stack(struct memory_map *map, uintptr_t heap_start, size_t stack_size)
{
	uintptr_t start;
	int error = ENOMEM;
	MEMORY_OBJECT_T *heap_object, *stack_object;

	#if defined(CONFIG_MMU)

	start = heap_start;
	// TODO this is for testing, it preallocated the stack
	//start = (uintptr_t) page_alloc_contiguous(stack_size);
	heap_object = NULL;
	stack_object = NULL;

	#else

	struct memory_region *region = NULL;

	region = memory_region_alloc_user_memory(stack_size, NULL);
	if (!region)
		return ENOMEM;

	start = (uintptr_t) region->mem_start;
	heap_object = MEMORY_OBJECT_MAKE_REF(region);
	stack_object = region;

	#endif

	error = memory_map_mmap(map, start, 0, SEG_TYPE_HEAP | SEG_READ | SEG_WRITE | SEG_ANONYMOUS, heap_object, 0);
	if (error < 0)
		goto fail;
	error = memory_map_mmap(map, start, stack_size, SEG_TYPE_STACK | SEG_GROWSDOWN | SEG_READ | SEG_WRITE | SEG_ANONYMOUS, stack_object, 0);
	if (error < 0)
		goto fail;
	map->heap_start = start;
	map->sbrk = start;
	map->stack_end = start + stack_size;

	return 0;

fail:
	#if !defined(CONFIG_MMU)
	if (region)
		memory_region_free(region);
	#endif
	return error;
}

void memory_map_print_segments(struct memory_map *map)
{
	struct memory_segment *cur;

	if (map) {
		if (_queue_head(&map->segments)) {
			for (cur = _queue_head(&map->segments); cur; cur = _queue_next(&cur->node)) {
				printk("%x to %x: flags=%x\n", cur->start, cur->end, cur->flags);
			}
		} else {
			printk("<map has no segments>\n");
		}
	} else {
		printk("<map is NULL>\n");
	}
}

