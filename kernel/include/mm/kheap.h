#ifndef _MM_KHEAP_H_
#define _MM_KHEAP_H_

#include <stddef.h>

void *kheap_alloc(size_t size, int page_aligned);
void kheap_free(void *p);
void kheap_init();

/* kernel heap position */
extern uint32_t kheap_pos;

#endif
