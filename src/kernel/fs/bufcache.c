
#include <stddef.h>
#include <string.h>

#include <kernel/printk.h>
#include <kernel/drivers.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/fs/bufcache.h>
#include <kernel/utils/queue.h>
#include <kernel/utils/iovec.h>


#define BUFCACHE_MAX		20


static inline struct buf *_find_free_entry(struct bufcache *cache);
static struct buf *_load_block(struct bufcache *cache, block_t num);
static inline struct buf *_create_entry(struct bufcache *cache);
static inline int _read_entry(struct bufcache *cache, struct buf *entry);
static inline int _write_entry(struct bufcache *cache, struct buf *entry);

void init_bufcache(struct bufcache *cache, device_t dev, int block_size)
{
	cache->dev = dev;
	cache->flags = 0;
	cache->block_size = block_size;
	_queue_init(&cache->blocks);
}

void free_bufcache(struct bufcache *cache)
{
	struct buf *cur, *next;

	for (cur = (struct buf *) cache->blocks.head; cur; cur = next) {
		next = (struct buf *) cur->node.next;
		kmfree(cur->block);
		kmfree(cur);
	}

	// Ensure it won't be used after being freed
	_queue_init(&cache->blocks);
}

void sync_bufcache(struct bufcache *cache)
{
	struct buf *cur;

	for (cur = (struct buf *) cache->blocks.head; cur; cur = (struct buf *) cur->node.next) {
		_write_entry(cache, cur);
	}
}

struct buf *get_block(struct bufcache *cache, block_t num)
{
	struct buf *cur;

	for (cur = _queue_head(&cache->blocks); cur; cur = _queue_next(&cur->node)) {
		if (cur->num == num) {
			cur->refcount++;

			// Move to the front of the cache list
			_queue_remove(&cache->blocks, &cur->node);
			_queue_insert(&cache->blocks, &cur->node);
			return cur;
		}
	}

	return _load_block(cache, num);
}

int release_block(struct buf *buf, short dirty)
{
	if (dirty) {
		mark_block_dirty(buf);
	}

	if (--buf->refcount == 0) {
		// TODO we actually maybe don't want to write until this entry is recycled, or else any bit changes will require an immediate writeback
		//_write_entry(cache->dev, buf);
	} else if (buf->refcount < 0) {
		buf->refcount = 0;
		log_warning("error: possible double free for block %d\n", buf->num);
	}
	return 0;
}

void mark_block_dirty(struct buf *buf)
{
	buf->flags |= BCF_DIRTY;
}

static struct buf *_load_block(struct bufcache *cache, block_t num)
{
	struct buf *entry;

	if (!cache->dev) {
		log_error("bufcache: device is set to %d\n", cache->dev);
		return NULL;
	}

	entry = _find_free_entry(cache);
	// TODO set errno?
	if (!entry) {
		return NULL;
	}

	entry->refcount = 1;
	entry->num = num;
	entry->flags = 0;

	if (_read_entry(cache, entry) < 0) {
		entry->refcount = 0;
		return NULL;
	}

	return entry;
}

static inline struct buf *_find_free_entry(struct bufcache *cache)
{
	int count = 0;
	struct buf *last;

	// Recycle the last used entry
	for (last = _queue_tail(&cache->blocks); last && last->refcount > 0; last = _queue_prev(&last->node), count++) { }

	if (!last) {
		if (count < BUFCACHE_MAX) {
			last = _create_entry(cache);
			if (!last)
				return NULL;
		} else {
			panic("Error: ran out of bufcache entries\n");
			return NULL;
		}
	}

	_queue_remove(&cache->blocks, &last->node);
	_queue_insert(&cache->blocks, &last->node);
	if (last->num != 0 && last->block) {
		_write_entry(cache, last);
	}
	return last;
}

static inline struct buf *_create_entry(struct bufcache *cache)
{
	void *block;
	struct buf *buf;

	buf = kmalloc(sizeof(struct buf));
	if (!buf)
		return NULL;
	block = kzalloc(cache->block_size);
	if (!block)
		return NULL;
	_queue_node_init(&buf->node);
	buf->refcount = 0;
	buf->flags = 0;
	buf->num = 0;
	buf->block = block;
	_queue_insert(&cache->blocks, &buf->node);
	return buf;
}

static inline int _read_entry(struct bufcache *cache, struct buf *entry)
{
	struct kvec kvec;
	struct iovec_iter iter;

	//printk("READING %x: %x <- %x x %x\n", entry->dev, entry->block, (entry->num * cache->block_size), cache->block_size);
	iovec_iter_init_simple_kvec(&iter, &kvec, entry->block, cache->block_size);
	int size = dev_read(cache->dev, (entry->num * cache->block_size), &iter);
	if (size != cache->block_size) {
		return -1;
	}
	return 0;
}

static inline int _write_entry(struct bufcache *cache, struct buf *entry)
{
	struct kvec kvec;
	struct iovec_iter iter;

	if (!(entry->flags & BCF_DIRTY)) {
		return 0;
	}
	//printk("WRITING %x: %x <- %x x %x\n", entry->dev, (entry->num * cache->block_size), entry->block, cache->block_size);
	iovec_iter_init_simple_kvec(&iter, &kvec, entry->block, cache->block_size);
	int size = dev_write(cache->dev, (entry->num * cache->block_size), &iter);
	if (size != cache->block_size) {
		return -1;
	}
	entry->flags &= ~BCF_DIRTY;
	return 1;
}

