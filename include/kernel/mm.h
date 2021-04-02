#ifndef _MM_H_
#define _MM_H_

#include <lib/stddef.h>

#define PAGE_SIZE       0x1000    /* 4 kB */

void init_mem(uint32_t start, uint32_t end);
uint32_t kmalloc_phys(uint32_t size, uint8_t align, uint32_t *phys);
void *kmalloc(uint32_t size);
void kfree(void *p);

#endif
