
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
	page_free_contiguous(object->mem_start, object->mem_length);
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


struct memory_area *memory_area_alloc(uintptr_t start, uintptr_t end, int flags, struct memory_object *object)
{
	struct memory_area *area;

	area = kmalloc(sizeof(struct memory_area));
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

void memory_area_free(struct memory_area *area)
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

	return map;
}

void memory_map_free(struct memory_map *map)
{
	struct memory_area *cur, *next;

	if (!map) {
		return;
	}

	if (--map->refcount == 0) {
		for (cur = _queue_head(&map->segments); cur; cur = next) {
			next = _queue_next(&cur->node);
			memory_area_free(cur);
		}
		kmfree(map);
		// TODO if CONFIG_MMU then also free table?
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
	struct memory_area *cur, *next, *area;

	end = start + length;

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

	area = memory_area_alloc(start, end, flags, object);
	if (!area) {
		error = ENOMEM;
		goto fail;
	}

	// If cur is NULL (ie. no space found after first segment), then insert it at the beginning
	_queue_insert_after(&map->segments, &area->node, cur ? &cur->node : NULL);

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
	uintptr_t end;
	struct memory_area *cur, *next, *new;

	end = start + length;
	for (cur = memory_map_iter_first(map); cur; cur = next) {
		next = memory_map_iter_next(cur);
		if (cur->start >= start && cur->start <= end) {
			if (cur->end >= start && cur->end <= end) {
				// The segment is entirely within the unmap region so delete it entirely
				_queue_remove(&map->segments, &cur->node);
				memory_area_free(cur);
			} else {
				// The start will be unmapped but not the end, so move the segment start
				cur->start = end;
			}
		} else if (cur->end >= start && cur->end <= end) {
			// The end will be unmapped but not the start, so move the segment end
			cur->end = start;
		} else if (start >= cur->start && start <= cur->end) {
			//  The unmapped region is entirely within this segment, so we need to split it in two
			new = memory_area_alloc(end, cur->end, cur->flags, memory_object_make_ref(cur->object));
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


/// Resize the given area
///
/// The segment will be resized by `diff` bytes.  If the segment has the MAP_GROWSDOWN
/// flag set, then it will grow the start of the segment downwards.  Otherwise the segment
/// will grow the end of the segment upwards.  If there is an adjacent segment that would
/// overlap after the change, then it is shrunk by the necessary amount
int memory_map_resize(struct memory_area *area, ssize_t diff)
{
	uintptr_t new_addr;
	struct memory_area *adjacent;

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
	struct memory_area *heap, *stack;

	new_sbrk = map->sbrk + diff;

	stack = memory_area_find_prev(memory_map_iter_last(map), map->sbrk);
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
	struct memory_area *cur;

	printk("pid %d memory map:\n", proc->pid);
	for (cur = _queue_head(&proc->map->segments); cur; cur = _queue_next(&cur->node)) {
		printk("%x to %x: flags=%x\n", cur->start, cur->end, cur->flags);
	}
}

