
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>

#include <kernel/net/socket.h>
#include <kernel/net/protocol.h>
#include <kernel/utils/queue.h>

#define PROTOCOLS_MAX		4


static struct protocol *proto_list[PROTOCOLS_MAX];


int init_net_protocol()
{
	for (short i = 0; i < PROTOCOLS_MAX; i++)
		proto_list[i] = NULL;
	return 0;
}

int net_register_protocol(struct protocol *proto)
{
	for (short i = 0; i < PROTOCOLS_MAX; i++) {
		if (!proto_list[i]) {
			proto_list[i] = proto;
			return 0;
		}
	}
	return -1;
}

struct protocol *net_get_protocol(int domain, int type, int protocol)
{
	for (short i = 0; i < PROTOCOLS_MAX; i++) {
		if (proto_list[i] && proto_list[i]->domain == domain && (!protocol || proto_list[i]->protocol == protocol) && (!type || proto_list[i]->type == type))
			return proto_list[i];
	}
	return NULL;
}

int net_incoming_packet(struct protocol *proto, struct packet *pack)
{
	int error;

	error = proto->ops->decode_header(proto, pack, 0);
	if (error) {
		packet_free(pack);
		return PACKET_ERROR;
	}

	error = proto->ops->forward_packet(proto, pack);
	if (error == PACKET_DELIVERED)
		return PACKET_DELIVERED;
	
	packet_free(pack);
	return PACKET_DROPPED;
}


int net_create_endpoint(struct protocol *proto, struct socket *sock, const struct sockaddr *sockaddr, socklen_t len, struct endpoint **result)
{
	if (!proto->ops->create_endpoint)
		return EAFNOSUPPORT;
	return proto->ops->create_endpoint(proto, sock, sockaddr, len, result);
}

int net_destroy_endpoint(struct endpoint *ep)
{
	return ep->ops->destroy(ep);
}

