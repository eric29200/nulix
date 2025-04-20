#ifndef _MM_KHEAP_H_
#define _MM_KHEAP_H_

#include <stddef.h>

void *kmalloc(uint32_t size);
void kfree(void *p);
void kheap_init();

#endif
