
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

#define rounddown(value, size)	((value) & ~((size) - 1))

#define KERNEL_STACK_SIZE	CONFIG_KERNEL_STACK_SIZE * PAGE_SIZE

#define SEG_TYPE		0x000F		// Mask for type of mapping
#define SEG_TYPE_INVALID	0x0000
#define SEG_TYPE_CODE		0x0001
#define SEG_TYPE_DATA		0x0002
#define SEG_TYPE_HEAP		0x0003
#define SEG_TYPE_STACK		0x0004

#define SEG_READ		0x0010
#define SEG_WRITE		0x0020
#define SEG_EXECUTABLE		0x0040

#define SEG_GROWSDOWN		0x0100		// stack-like segment

#define SEG_ANONYMOUS		0x1000
#define SEG_FIXED		0x2000
#define SEG_POPULATE		0x4000

struct vfile;
struct memory_region;
struct memory_segment;

struct memory_ops {
	page_t *(*load_page_at)(struct memory_segment *segment, virtual_address_t vaddr);
};

#if !defined(CONFIG_MMU)
/// Represents a contiguous piece of physical memory backing a program segment
///
/// By having a separately allocated region, it shared between processes and refcounted
/// as a block instead of per-page.  When using an MMU, per-page refcounts will be kept
/// but in no-MMU systems, per-page refcounts will not be stored and only the
/// memory_region refcount will be used
struct memory_region {
	int refcount;

	/// The open file that was used to load data into this region.  (could be NULL)
	struct vfile *file;
	/// Start of pre-allocated memory region
	void *mem_start;
	/// Length of pre-allocated memory region
	size_t mem_length;
};
#endif

struct memory_segment {
	struct queue_node node;

	int flags;
	uintptr_t start;
	uintptr_t end;

	#if defined(CONFIG_MMU)

	/// The memory operations used to load data into this segment when a page miss occurs
	struct memory_ops *ops;
	/// The open file used for file-backed segments.  (could be NULL)
	struct vfile *file;

	#else

	/// The pre-allocated region of memory that this segment looks into
	struct memory_region *region;

	#endif

	/// The offset into either the file, or the memory region where this segment starts
	offset_t offset;
};

struct memory_map {
	int refcount;

	#if defined(CONFIG_MMU)
	/// Root page table used by the MMU that represents this memory map
	mmu_descriptor_t *root_table;
	#endif

	struct queue segments;
	uintptr_t code_start;
	uintptr_t data_start;
	uintptr_t heap_start;
	uintptr_t sbrk;
	uintptr_t stack_end;

	const char *const *argv;
	const char *const *envp;
};


#if defined(CONFIG_MMU)
#define MEMORY_OBJECT_T				struct vfile
#define MEMORY_OBJECT_ACCESS(cur)		((cur)->file)
#define MEMORY_OBJECT_MAKE_REF(object)		vfs_clone_fileptr((object))
#define MEMORY_OBJECT_FREE(object)		vfs_close((object))
#else
#define MEMORY_OBJECT_T				struct memory_region
#define MEMORY_OBJECT_ACCESS(cur)		((cur)->region)
#define MEMORY_OBJECT_MAKE_REF(object)		memory_region_make_ref((object))
#define MEMORY_OBJECT_FREE(object)		memory_region_free((object))
#endif

#if !defined(CONFIG_MMU)
struct memory_region *memory_region_alloc_user_memory(size_t size, struct vfile *file);
void memory_region_free(struct memory_region *region);
#endif

struct memory_map *memory_map_alloc(void);
void memory_map_free(struct memory_map *map);
static inline struct memory_map *memory_map_make_ref(struct memory_map *map);

int memory_map_mmap(struct memory_map *map, uintptr_t start, size_t length, int flags, MEMORY_OBJECT_T *object, offset_t offset);
int memory_map_unmap(struct memory_map *map, uintptr_t start, size_t length);
int memory_map_copy_segment(struct memory_map *dest_map, struct memory_map *src_map, struct memory_segment *src_segment);

int memory_map_resize(struct memory_segment *segment, ssize_t diff);
int memory_map_insert_heap_stack(struct memory_map *map, uintptr_t heap_start, size_t stack_size);

int memory_map_move_sbrk(struct memory_map *map, int diff);

int memory_map_load_page_at(struct memory_map *map, virtual_address_t vaddr, uint16_t write_flag);

void memory_map_print_segments(struct memory_map *map);


/************************
 * Memory Region Inlines
 ************************/

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

#if !defined(CONFIG_MMU)
static inline struct memory_region *memory_region_make_ref(struct memory_region *region)
{
	region->refcount++;
	return region;
}

static inline uintptr_t memory_region_get_start_address(struct memory_region *region)
{
	return (uintptr_t) region->mem_start;
}
#endif


/*********************
 * Memory Map Inlines
 *********************/

static inline struct memory_segment *memory_map_iter_first(struct memory_map *map)
{
	return _queue_head(&map->segments);
}

static inline struct memory_segment *memory_map_iter_next(struct memory_segment *cur)
{
	return _queue_next(&cur->node);
}


static inline struct memory_segment *memory_map_iter_last(struct memory_map *map)
{
	return _queue_tail(&map->segments);
}

static inline struct memory_segment *memory_map_iter_prev(struct memory_segment *cur)
{
	return _queue_prev(&cur->node);
}

static inline struct memory_segment *memory_segment_find_next(struct memory_segment *cur, uintptr_t address)
{
	while (cur) {
		if (address >= cur->start && address < cur->end) {
			return cur;
		}
		cur = _queue_next(&cur->node);
	}
	return NULL;
}

static inline struct memory_segment *memory_segment_find_prev(struct memory_segment *cur, uintptr_t address)
{
	while (cur) {
		if (address >= cur->start && address < cur->end) {
			return cur;
		}
		cur = _queue_prev(&cur->node);
	}
	return NULL;
}

static inline struct memory_segment *memory_map_find(struct memory_map *map, uintptr_t address)
{
	return memory_segment_find_next(memory_map_iter_first(map), address);
}

#endif
