
#include <errno.h>
#include <string.h>

#include <kernel/mm/kmalloc.h>
#include <kernel/net/if.h>
#include <kernel/net/packet.h>
#include <kernel/utils/queue.h>
#include <kernel/utils/iovec.h>


struct packet *packet_alloc(struct if_device *ifdev, struct protocol *proto, size_t capacity)
{
	struct packet *pack;

	pack = kzalloc(sizeof(struct packet) + capacity);
	if (!pack) {
		return NULL;
	}

	_queue_node_init(&pack->node);
	pack->proto = proto;
	pack->ifdev = ifdev;
	pack->capacity = capacity;

	return pack;
}

void packet_free(struct packet *pack)
{
	// TODO should you search through the linked list and free all??
	// TODO if headers are allocated separately, then free them
	kmfree(pack);
}

int packet_append(struct packet *pack, const void *ptr, int nbytes)
{
	if (pack->length + nbytes > pack->capacity) {
		return ENOMEM;
	}
	memcpy(&pack->data[pack->length], ptr, nbytes);
	pack->length += nbytes;
	return 0;
}

int packet_append_iovec(struct packet *pack, struct iovec_iter *iter)
{
	size_t nbytes;

	nbytes = iovec_iter_length(iter);
	if (pack->length + nbytes > pack->capacity) {
		return ENOMEM;
	}
	nbytes = memcpy_out_of_iter(iter, &pack->data[pack->length], nbytes);
	if (nbytes < 0) {
		return nbytes;
	}
	pack->length += nbytes;
	return 0;
}

