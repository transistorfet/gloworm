
#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

#define RAND_MAX	32767


int atoi(const char *str);
char *itoa(int num, char *buffer, int radix);
long int strtol(const char *str, char **endptr, int base);

void init_heap(void *addr, unsigned long size);

void *malloc(size_t size);
void free(void *ptr);

void srand(unsigned int seed);
int rand(void);

#endif
