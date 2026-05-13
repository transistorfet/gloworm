
#ifndef _KERNEL_FS_BLOCKCACHE_H
#define _KERNEL_FS_BLOCKCACHE_H

#include <stddef.h>
#include <sys/types.h>
#include <asm/addresses.h>
#include <kernel/utils/iovec.h>
#include <kernel/utils/queue.h>


#define BCF_DIRTY	0x01

typedef unsigned int block_t;

struct bufcache {
	device_t dev;
	short flags;
	int block_size;
	struct queue blocks;
};

struct buf {
	struct queue_node node;
	int refcount;
	short flags;
	//device_t dev;
	block_t num;
	void *block;
	//physical_address_t page;
	//int num_sectors;
	//struct kvec sectors[];
};

void init_bufcache(struct bufcache *cache, device_t dev, int block_size);
void free_bufcache(struct bufcache *cache);
void sync_bufcache(struct bufcache *cache);
struct buf *get_block(struct bufcache *cache, block_t num);
int release_block(struct buf *buf, short dirty);
void mark_block_dirty(struct buf *buf);

#endif
