#ifndef _MM_H_
#define _MM_H_

#include <stddef.h>
#include <mm/paging.h>

#define PAGE_SIZE               0x1000

#define KMEM_SIZE               0x600000

#define KHEAP_START             0x400000
#define KHEAP_SIZE              0x200000

void init_mem(uint32_t start, uint32_t end);
void *kmalloc(uint32_t size);
void *kmalloc_align(uint32_t size);
void *kmalloc_phys(uint32_t size, uint8_t align, uint32_t *phys);
void kfree(void *p);

#endif
