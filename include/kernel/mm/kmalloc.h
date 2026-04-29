
#ifndef _KERNEL_MM_KMALLOC_H
#define _KERNEL_MM_KMALLOC_H

#include <stdint.h>

int init_kernel_heap(void);
int kernel_heap_set_private_page_block(uintptr_t start, uintptr_t end);
void *kzalloc(uintptr_t size);
void *kmalloc(uintptr_t size);
void kmfree(void *ptr);
void kernel_heap_compact(void);

#endif
