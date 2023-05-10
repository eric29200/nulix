#ifndef _LIBC_STDLIB_H_
#define _LIBC_STDLIB_H_

#include <stdio.h>

#define EXIT_FAILURE	1
#define EXIT_SUCCESS	0

void exit(int status);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

int atoi(const char *s);

void qsort(void *base, size_t nmemb, size_t size, int (*compare)(const void *, const void *));

#endif
