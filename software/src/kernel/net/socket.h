
#ifndef _SRC_KERNEL_NET_SOCKET_H
#define _SRC_KERNEL_NET_SOCKET_H

#include <kernel/vfs.h>

#include "../misc/queue.h"

// TODO this is supposed to be the largest possible sockaddr.  Right now it's hard coded to the size of struct sockaddr
struct sockaddr_storage {
	char _padding[16];
};

#define SOCKET(vnode)		(&((struct sock_vnode *) (vnode))->sock)

struct socket {
	struct queue_node node;

	uint8_t domain;
	uint8_t type;
	uint8_t state;
	uint8_t syscall;

	struct sockaddr_storage local;
	struct sockaddr_storage remote;
	struct protocol *proto;
	struct queue recv_queue;
};


int net_socket_create(int domain, int type, int protocol, uid_t uid, struct vfile **file);
int net_socket_release_vnode(struct vnode *vnode);
int net_socket_close(struct vfile *file);
int net_socket_read(struct vfile *file, char *buf, size_t nbytes);
int net_socket_write(struct vfile *file, const char *buf, size_t nbytes);
int net_socket_bind(struct vfile *file, const struct sockaddr *addr, socklen_t len);
int net_socket_connect(struct vfile *file, const struct sockaddr *addr, socklen_t len);
ssize_t net_socket_send(struct vfile *file, const void *buf, size_t n, int flags);
ssize_t net_socket_sendto(struct vfile *file, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len);
ssize_t net_socket_recv(struct vfile *file, void *buf, size_t n, int flags);
ssize_t net_socket_recvfrom(struct vfile *file, void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t *addr_len);
int net_socket_wakeup(struct socket *sock);

#endif
