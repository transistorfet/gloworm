
#ifndef _KERNEL_NET_PACKET_H
#define _KERNEL_NET_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/utils/queue.h>

#define PACKET_CUSTOM_DATA	32

struct socket;
struct protocol;
struct if_device;
struct iovec_iter;

struct packet {
	struct queue_node node;
	struct protocol *proto;
	struct if_device *ifdev;

	int network_offset;
	int transport_offset;
	int data_offset;

	char custom_data[PACKET_CUSTOM_DATA];

	int length;
	int capacity;
	unsigned char data[0];
};

struct packet *packet_alloc(struct if_device *ifdev, struct protocol *proto, size_t capacity);
void packet_free(struct packet *pack);
int packet_append(struct packet *pack, const void *buf, int nbytes);
int packet_append_iovec(struct packet *pack, struct iovec_iter *iter);

#endif
