
#ifndef _KERNEL_MM_KMALLOC_H
#define _KERNEL_MM_KMALLOC_H

#include <stdint.h>

void init_kernel_heap(uintptr_t start, uintptr_t end);
void *kzalloc(uintptr_t size);
void *kmalloc(uintptr_t size);
void kmfree(void *ptr);

#endif
