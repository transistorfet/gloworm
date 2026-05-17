
#include <stdlib.h>
#include <assert.h>

#include <kernel/fs/bufcache.h>

#define MAX_TEST_BUFS		100

int mock_block_size = 0;
struct buf mock_bufs[MAX_TEST_BUFS];

void init_bufcache(struct bufcache *cache, device_t dev, int block_size)
{
	mock_block_size = block_size;

	for (int i = 0; i < MAX_TEST_BUFS; i++) {
		_queue_node_init(&mock_bufs[i].node);
		mock_bufs[i].refcount = 0;
		mock_bufs[i].flags = 0;
		mock_bufs[i].num = i;
		mock_bufs[i].block = malloc(block_size);
	}
}

void free_bufcache(struct bufcache *cache)
{
	log_info("free_bufcache called\n");
	for (int i = 0; i < MAX_TEST_BUFS; i++) {
		free(mock_bufs[i].block);
	}
}

void sync_bufcache(struct bufcache *cache)
{
	log_info("sync_bufcache called\n");
}

struct buf *get_block(struct bufcache *cache, block_t num)
{
	if (num > MAX_TEST_BUFS) {
		log_error("error: invalid block number %d\n", num);
		return NULL;
	}

	mock_bufs[num].refcount += 1;
	return &mock_bufs[num];
}

int release_block(struct buf *buf, short dirty)
{
	if (--buf->refcount == 0) {
		log_info("would write back block %d\n", buf->num);
	} else if (buf->refcount < 0) {
		log_error("error: double free for block %d\n", buf->num);
		assert(buf->refcount == 0);
	}
	return 0;
}

void mark_block_dirty(struct buf *buf)
{
	buf->flags |= BCF_DIRTY;
}

