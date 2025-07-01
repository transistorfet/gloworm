
#ifndef _KERNEL_PROC_MEMORY_H
#define _KERNEL_PROC_MEMORY_H

#include <stddef.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <kconfig.h>
#include <asm/addresses.h>
#include <kernel/utils/queue.h>
#include <kernel/mm/pages.h>
#include <kernel/mm/kmalloc.h>

#if defined(CONFIG_MMU)
#include <asm/mmu.h>
#endif

#define KERNEL_STACK_SIZE	CONFIG_KERNEL_STACK_SIZE * PAGE_SIZE

#define AREA_TYPE		0x000F		// Mask for type of mapping
#define AREA_TYPE_INVALID	0x0000
#define AREA_TYPE_CODE		0x0001
#define AREA_TYPE_DATA		0x0002
#define AREA_TYPE_HEAP		0x0003
#define AREA_TYPE_STACK		0x0004

#define AREA_READ		0x0010
#define AREA_WRITE		0x0020
#define AREA_EXECUTABLE		0x0040

#define AREA_GROWSDOWN		0x0100		// stack-like segment

struct vfile;
struct process;
struct memory_area;
struct memory_object;

struct memory_ops {
	void (*free)(struct memory_object *object);
	physical_address_t (*load_page_at)(struct memory_area *area, struct memory_object *object, virtual_address_t vaddr);
};

struct file_backed_memory {
	/// The open file used for file-backed regions.  (could be NULL)
	struct vfile *file;
	offset_t file_offset;
};

struct anonymous_memory {
	physical_address_t address;
};

/// Represents a contiguous piece of physical memory backing a program segment
///
/// By having a separately allocated object, it shared between processes and refcounted
/// as a block instead of per-page.  When using an MMU, per-page refcounts will be kept
/// but in no-MMU systems, per-page refcounts will not be stored and only the
/// memory_object refcount will be used
struct memory_object {
	struct memory_ops *ops;

	int refcount;
	union {
		struct file_backed_memory file_backed;
		struct anonymous_memory anonymous;
	};

	#if !defined(CONFIG_MMU)
	void *mem_start;
	size_t mem_length;
	#endif
};

struct memory_area {
	struct queue_node node;

	int flags;
	uintptr_t start;
	uintptr_t end;

	struct memory_object *object;
};

struct memory_map {
	int refcount;

	struct queue segments;
	uintptr_t code_start;
	uintptr_t data_start;
	uintptr_t heap_start;
	uintptr_t sbrk;
	uintptr_t stack_end;

	const char *const *argv;
	const char *const *envp;

	#if defined(CONFIG_MMU)
	mmu_descriptor_t *root_table;
	#endif
};


struct memory_object *memory_object_alloc(struct vfile *file);
static inline struct memory_object *memory_object_make_ref(struct memory_object *object);
void memory_object_free(struct memory_object *object);
struct memory_object *memory_object_alloc_user_memory(size_t size, struct vfile *file);

struct memory_map *memory_map_alloc(void);
void memory_map_free(struct memory_map *map);
static inline struct memory_map *memory_map_make_ref(struct memory_map *map);

int memory_map_mmap(struct memory_map *map, uintptr_t start, size_t length, int flags, struct memory_object *object);
int memory_map_unmap(struct memory_map *map, uintptr_t start, size_t length);

int memory_map_resize(struct memory_area *area, ssize_t diff);
int memory_map_insert_heap_stack(struct memory_map *map, size_t stack_size);

int memory_map_move_sbrk(struct memory_map *map, int diff);

void print_process_segments(struct process *proc);


static inline int memory_map_reset_sbrk(struct memory_map *map)
{
	// Reset sbrk to the start of the heap
	return memory_map_move_sbrk(map, -1 * (map->sbrk - map->heap_start));
}

static inline struct memory_map *memory_map_make_ref(struct memory_map *map)
{
	map->refcount++;
	return map;
}

static inline struct memory_object *memory_object_make_ref(struct memory_object *object)
{
	object->refcount++;
	return object;
}

static inline struct memory_area *memory_map_iter_first(struct memory_map *map)
{
	return _queue_head(&map->segments);
}

static inline struct memory_area *memory_map_iter_next(struct memory_area *cur)
{
	return _queue_next(&cur->node);
}


static inline struct memory_area *memory_map_iter_last(struct memory_map *map)
{
	return _queue_tail(&map->segments);
}

static inline struct memory_area *memory_map_iter_prev(struct memory_area *cur)
{
	return _queue_prev(&cur->node);
}

static inline struct memory_area *memory_area_find_prev(struct memory_area *cur, uintptr_t address)
{
	while (cur) {
		if (address >= cur->start && address <= cur->end) {
			return cur;
		}
		cur = _queue_prev(&cur->node);
	}
	return NULL;
}

#endif
