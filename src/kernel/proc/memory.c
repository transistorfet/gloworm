
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <kconfig.h>
#include <asm/types.h>
#include <kernel/printk.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/mm/pages.h>
#include <kernel/fs/fileptr.h>
#include <kernel/proc/memory.h>
#include <kernel/arch/context.h>
#include <kernel/proc/process.h>


void pages_object_free(struct memory_object *object)
{
	#if defined(CONFIG_MMU)
	// TODO nothing to do, since unmap should do this for us?
	#else
	page_free_contiguous(object->mem_start, object->mem_length);
	#endif
}

struct memory_ops pages_object_ops = {
	.free		= pages_object_free,
	.load_page_at	= NULL,
};

struct memory_object *memory_object_alloc(struct vfile *file)
{
	struct memory_object *object;
	struct memory_ops *ops = NULL;

	ops = &pages_object_ops;

	object = kzalloc(sizeof(struct memory_object));
	if (!object) {
		return NULL;
	}

	object->refcount = 1;
	object->ops = ops;
	object->file_backed.file = file;

	return object;
}

void memory_object_free(struct memory_object *object)
{
	if (!object)
		return;

	if (--object->refcount == 0) {
		if (object->ops && object->ops->free) {
			object->ops->free(object);
		}
		kmfree(object);
	}
}

struct memory_object *memory_object_alloc_user_memory(size_t size, struct vfile *file)
{
	char *memory = NULL;
	struct memory_object *object;

	// Round the size up to the nearest page
	if ((size & (PAGE_SIZE - 1)) != 0) {
		size = (size & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
	}

	object = memory_object_alloc(file);
	if (!object)
		goto fail;

	#if !defined(CONFIG_MMU)
	memory = (char *) page_alloc_contiguous(size);
	if (!memory)
		goto fail;

	object->ops = &pages_object_ops;
	object->mem_start = memory;
	object->mem_length = size;
	object->anonymous.address = (physical_address_t) memory;
	#endif

	return object;

fail:
	if (memory)
		kmfree(memory);
	if (file)
		free_fileptr(file);
	return NULL;
}


struct memory_segment *memory_segment_alloc(uintptr_t start, uintptr_t end, int flags, struct memory_object *object)
{
	struct memory_segment *area;

	area = kmalloc(sizeof(struct memory_segment));
	if (!area) {
		memory_object_free(object);
		return NULL;
	}

	_queue_node_init(&area->node);
	area->flags = flags;
	area->start = start;
	area->end = end;
	area->object = object;

	return area;
}

void memory_segment_free(struct memory_segment *area)
{
	memory_object_free(area->object);
	kmfree(area);
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

	#if defined(CONFIG_MMU)
	if (map->root_table) {
		mmu_table_free(map->root_table);
	}
	#endif

	if (--map->refcount == 0) {
		for (cur = _queue_head(&map->segments); cur; cur = next) {
			next = _queue_next(&cur->node);
			memory_segment_free(cur);
		}
		kmfree(map);
	}
}

/// Map the given address range to the given object
///
/// NOTE: this will not unmap any previously mapped regions nor will it check for
/// existing mappings, so it should be paired with an unmap call when modifying
/// an existing map
int memory_map_mmap(struct memory_map *map, uintptr_t start, size_t length, int flags, struct memory_object *object)
{
	int error = 0;
	uintptr_t end;
	struct memory_segment *cur, *next, *area;

	end = start + length;

	// Check that the length is a multiple of the page size, or raise an error
	if (length & (PAGE_SIZE - 1)) {
		return EINVAL;
	}

	// Check that the virtual address given is aligned to a page, and raise an error if it's not
	if (start & (PAGE_SIZE - 1)) {
		return EINVAL;
	}

	// Check if the segment to be mapped will wrap around at the end of the address space, and
	// raise an error if it will
	if (start + length > 0 && start + length < start) {
		return EINVAL;
	}

	if (!object) {
		return EFAULT;
	}

	// Find the location to insert the new segment before allocating it
	for (cur = _queue_head(&map->segments); cur; cur = next) {
		next = _queue_next(&cur->node);
		if (cur->end <= start && (!next || end <= next->start)) {
			break;
		}
	}

	area = memory_segment_alloc(start, end, flags, object);
	if (!area) {
		error = ENOMEM;
		goto fail;
	}

	// If cur is NULL (ie. no space found after first segment), then insert it at the beginning
	_queue_insert_after(&map->segments, &area->node, cur ? &cur->node : NULL);

	#if defined(CONFIG_MMU)
	// TODO the flag here should be changed to MMU_FLAG_UNALLOCATED when it's working properly
	error = mmu_table_map(map->root_table, start, length, MMU_FLAG_WINDOW);
	if (error < 0)
		goto fail;
	#endif

	// Adjust memory markers, if needed
	if (!map->code_start) {
		if (flags & AREA_EXECUTABLE) {
			map->code_start = start;
		} else {
			map->data_start = start;
		}
	}

	if (end > map->stack_end) {
		map->stack_end = end;
	}

	return 0;

fail:
	if (object) {
		memory_object_free(object);
	}
	return error;
}

/// Unmap the given address range
///
/// If a segment is larger than the given range, it will be split in two.  Otherwise
/// it will be deleted or shrunk accordingly to make room.
int memory_map_unmap(struct memory_map *map, uintptr_t start, size_t length)
{
	int error;
	uintptr_t end;
	struct memory_segment *cur, *next, *new;

	#if defined(CONFIG_MMU)
	// TODO the MMU_FLAG_PAGE_BACKED flag is a bit of a hack atm to make it not free the underlying data when using MMU_FLAG_WINDOW
	error = mmu_table_map(map->root_table, start, length, MMU_FLAG_UNMAP | MMU_FLAG_PAGE_BACKED);
	if (error < 0)
		return error;
	#endif

	end = start + length;
	for (cur = memory_map_iter_first(map); cur; cur = next) {
		next = memory_map_iter_next(cur);
		if (cur->start >= start && cur->start <= end) {
			if (cur->end >= start && cur->end <= end) {
				// The segment is entirely within the unmap region so delete it entirely
				_queue_remove(&map->segments, &cur->node);
				memory_segment_free(cur);
			} else {
				// The start will be unmapped but not the end, so move the segment start
				cur->start = end;
			}
		} else if (cur->end >= start && cur->end <= end) {
			// The end will be unmapped but not the start, so move the segment end
			cur->end = start;
		} else if (start >= cur->start && start <= cur->end) {
			//  The unmapped region is entirely within this segment, so we need to split it in two
			new = memory_segment_alloc(end, cur->end, cur->flags, memory_object_make_ref(cur->object));
			if (!new)
				return ENOMEM;
			_queue_insert_after(&map->segments, &new->node, &cur->node);
			cur->end = start;
		}
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

int memory_map_remap_copy_on_write(struct memory_map *map, uintptr_t start, size_t length)
{

	// TODO would this have to be one exact segment entry to work?
	// TODO well I guess the page loading strategy function could be the same and check
	//	the page descriptor for a read only bit, while the segment would be read write
	//	I guess this could skip any segments that are read only, since write isn't needed?
	// TODO so this would primarily be a pages.c impl for remapping, and it's mostly touching
	//	the page descriptors, but would also increment all refcounts?
	// TODO actually, should it be the caller (fork.c) to increment the page references?

	return 0;
}


/// Resize the given area
///
/// The segment will be resized by `diff` bytes.  If the segment has the MAP_GROWSDOWN
/// flag set, then it will grow the start of the segment downwards.  Otherwise the segment
/// will grow the end of the segment upwards.  If there is an adjacent segment that would
/// overlap after the change, then it is shrunk by the necessary amount
int memory_map_resize(struct memory_segment *area, ssize_t diff)
{
	uintptr_t new_addr;
	struct memory_segment *adjacent;

	if (area->flags & AREA_GROWSDOWN) {
		new_addr = area->start - diff;
		adjacent = _queue_prev(&area->node);
		if (adjacent && adjacent->end > new_addr) {
			// TODO should this be an error or should it move it?
			if (adjacent->start > new_addr) {
				return EFAULT;
			}
			adjacent->end = new_addr;
		}
		area->start = new_addr;
	} else {
		new_addr = area->end + diff;
		adjacent = _queue_next(&area->node);
		if (adjacent && new_addr > adjacent->start) {
			// TODO should this be an error or should it move it?
			if (new_addr > adjacent->end) {
				return EFAULT;
			}
			adjacent->start = new_addr;
		}
		area->end = new_addr;
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
	if ((diff > 0 ? diff : -diff) & (PAGE_SIZE - 1)) {
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


int memory_map_insert_heap_stack(struct memory_map *map, size_t stack_size)
{
	uintptr_t start;
	int error = ENOMEM;
	struct memory_object *object = NULL;

	object = memory_object_alloc_user_memory(stack_size, NULL);
	if (!object)
		return ENOMEM;

	start = object->anonymous.address;
	error = memory_map_mmap(map, start, 0, AREA_TYPE_HEAP | AREA_READ | AREA_WRITE, memory_object_make_ref(object));
	if (error < 0)
		goto fail;
	error = memory_map_mmap(map, start, stack_size, AREA_TYPE_STACK | AREA_GROWSDOWN | AREA_READ | AREA_WRITE, memory_object_make_ref(object));
	if (error < 0)
		goto fail;
	map->heap_start = start;
	map->sbrk = start;
	map->stack_end = start + stack_size;

	// Release the extra reference before exiting
	memory_object_free(object);

	return 0;

fail:
	if (object)
		memory_object_free(object);
	return error;
}

void print_process_segments(struct process *proc)
{
	struct memory_segment *cur;

	if (proc->map) {
		printk("pid %d memory map:\n", proc->pid);
		for (cur = _queue_head(&proc->map->segments); cur; cur = _queue_next(&cur->node)) {
			printk("%x to %x: flags=%x\n", cur->start, cur->end, cur->flags);
		}
	} else {
		printk("pid %d memory map: NULL\n", proc->pid);
	}
}

