
#ifndef INCLUDE_KERNEL_KMALLOC_H
#define INCLUDE_KERNEL_KMALLOC_H

#include <stdint.h>

void init_kernel_heap(uintptr_t start, uintptr_t end);
void *kmalloc(int size);
void kmfree(void *ptr);

#endif
