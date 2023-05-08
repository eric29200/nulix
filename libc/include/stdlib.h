#ifndef _LIBC_STDLIB_H_
#define _LIBC_STDLIB_H_

#include <stdio.h>

void exit(int status);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

#endif