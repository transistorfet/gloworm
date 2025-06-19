
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "../include/kernel/utils/queue.h"

struct queue queue1;
struct queue queue2;


void print_queue(struct queue *queue)
{
	for (struct queue_node *cur = _queue_head(queue); cur; cur = _queue_next(cur)) {
		printf("node %lx, ", (unsigned long) cur);
	}
	printf("\n");
}

int main()
{
	struct queue_node node1;
	struct queue_node node2;
	struct queue_node node3;

	_queue_init(&queue1);
	_queue_init(&queue2);

	_queue_insert(&queue1, &node1);
	assert(_queue_head(&queue1) == &node1);

	_queue_insert(&queue1, &node2);
	assert(_queue_head(&queue1) == &node2);
	assert(_queue_next(&node2) == &node1);

	_queue_insert_after(&queue1, &node3, _queue_tail(&queue1));
	assert(_queue_head(&queue1) == &node2);
	assert(_queue_next(&node2) == &node1);
	assert(_queue_next(&node1) == &node3);

	print_queue(&queue1);

	_queue_remove(&queue1, &node1);
	_queue_remove(&queue1, &node2);
	assert(_queue_head(&queue1) == &node3);

	_queue_insert(&queue2, &node1);


	print_queue(&queue1);

	printf("done\n");

	print_queue(&queue2);

	return 0;
}

