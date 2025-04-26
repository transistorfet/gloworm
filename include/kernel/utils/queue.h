
#ifndef _KERNEL_UTILS_QUEUE_H
#define _KERNEL_UTILS_QUEUE_H

#include <stddef.h>

struct queue {
	struct queue_node *head;
	struct queue_node *tail;
};

struct queue_node {
	struct queue_node *next;
	struct queue_node *prev;
};


static inline void _queue_init(struct queue *queue)
{
	queue->head = NULL;
	queue->tail = NULL;
}

static inline void _queue_node_init(struct queue_node *node)
{
	node->next = NULL;
	node->prev = NULL;
}

static inline void _queue_insert(struct queue *queue, struct queue_node *node)
{
	if (queue->head)
		queue->head->prev = node;
	else
		queue->tail = node;
	node->next = queue->head;
	node->prev = NULL;
	queue->head = node;
}

static inline void _queue_insert_after(struct queue *queue, struct queue_node *node, struct queue_node *after)
{
	struct queue_node *tail;

	// If `after` is NULL then insert at the start of the list (ie. queue->head)
	if (after)
		tail = after->next;
	else
		tail = queue->head;

	// Connect the tail of the list to the node
	if (tail)
		tail->prev = node;
	else
		queue->tail = node;
	node->next = tail;

	// Connect the list up to and including `after` to the node
	if (after)
		after->next = node;
	else
		queue->head = node;
	node->prev = after;
}

static inline void _queue_remove(struct queue *queue, struct queue_node *node)
{
	if (node->next)
		node->next->prev = node->prev;
	else
		queue->tail = node->prev;

	if (node->prev)
		node->prev->next = node->next;
	else
		queue->head = node->next;

	node->next = NULL;
	node->prev = NULL;
}

static inline void *_queue_head(struct queue *queue)
{
       return queue->head;
}

static inline void *_queue_tail(struct queue *queue)
{
       return queue->tail;
}

static inline void *_queue_next(struct queue_node *node)
{
       return node->next;
}

static inline void *_queue_prev(struct queue_node *node)
{
       return node->prev;
}

#endif

