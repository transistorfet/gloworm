
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <kernel/utils/macros.h>

#include <kernel/utils/queue.h>

struct queue queue1;
struct queue queue2;


void print_queue(struct queue *queue)
{
	for (struct queue_node *cur = _queue_head(queue); cur; cur = _queue_next(cur)) {
		printf("node %lx, ", (unsigned long) cur);
	}
	printf("\n");
}

int test_queue_insert_remove(void)
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

struct test_item {
	uint32_t some_data;
	uint32_t some_more_data;
	struct queue_node list1_node;
	struct queue_node list2_node;
};

int test_queue_container_of(void)
{
	struct queue queue1;
	struct queue queue2;

	struct test_item item1 = { 1 };
	struct test_item item2 = { 2 };
	struct test_item item3 = { 3 };
	struct test_item item4 = { 4 };

	_queue_init(&queue1);
	_queue_init(&queue2);

	_queue_insert(&queue1, &item1.list1_node);
	assert(_queue_head(&queue1) == &item1.list1_node);
	assert(_queue_head(&queue1) != &item1);

	_queue_insert_after(&queue1, &item2.list1_node, _queue_tail(&queue1));
	_queue_insert_after(&queue1, &item3.list1_node, _queue_tail(&queue1));
	_queue_insert_after(&queue1, &item4.list1_node, _queue_tail(&queue1));

	_queue_insert(&queue2, &item4.list2_node);
	assert(_queue_head(&queue2) == &item4.list2_node);
	assert(_queue_head(&queue2) != &item4);

	struct queue_node *node = _queue_head(&queue1);
	struct test_item *item = container_of(struct test_item, node, list1_node);
	printf("item %lx with data %d\n", (uintptr_t) item, item->some_data);
	assert(item == &item1);

	node = _queue_next(node);
	item = container_of(struct test_item, node, list1_node);
	printf("item %lx with data %d\n", (uintptr_t) item, item->some_data);
	assert(item == &item2);

	node = _queue_next(node);
	item = container_of(struct test_item, node, list1_node);
	printf("item %lx with data %d\n", (uintptr_t) item, item->some_data);
	assert(item == &item3);

	node = _queue_next(node);
	item = container_of(struct test_item, node, list1_node);
	printf("item %lx with data %d\n", (uintptr_t) item, item->some_data);
	assert(item == &item4);

	node = _queue_head(&queue2);
	item = container_of(struct test_item, node, list2_node);
	printf("item %lx with data %d\n", (uintptr_t) item, item->some_data);
	assert(item == &item4);

	return 0;
}

#define run(test) \
	printf("Running %s\n", #test); \
	assert(test() == 0);

int main(void)
{
	run(test_queue_insert_remove);
	run(test_queue_container_of);

	printf("%s tests passed\n", __FILE_NAME__);

	return 0;
}

