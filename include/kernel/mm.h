#ifndef _MM_H_
#define _MM_H_

#include <stddef.h>

#define PAGE_SIZE       0x1000    /* 4 kB */

void init_mem(uint32_t start, uint32_t end);
void *kmalloc(uint32_t size);
void *kmalloc_align(uint32_t size);
void *kmalloc_align_phys(uint32_t size, uint32_t *phys);
void kfree(void *p);

#endif
