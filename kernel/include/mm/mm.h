#ifndef _MM_H_
#define _MM_H_

#include <stddef.h>
#include <mm/paging.h>

#define PAGE_SIZE               0x1000

#define KHEAP_START             0xC0000000
#define KHEAP_INIT_SIZE         0x100000
#define KHEAP_MAX_SIZE          0x2000000
#define HEAP_EXPANSION_SIZE     0x100000

#define KPAGE_START             0xC3000000

void init_mem(uint32_t start, uint32_t end);
void *kmalloc(uint32_t size);
void *kmalloc_align(uint32_t size);
void *kmalloc_align_phys(uint32_t size, uint32_t *phys);
void *alloc_page();
void free_page(void *page);
void map_page(struct page_directory_t *pgd, uint32_t vaddr, uint32_t paddr, uint8_t kernel, uint8_t write);
void kfree(void *p);

#endif
