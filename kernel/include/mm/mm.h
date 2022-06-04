#ifndef _MM_H_
#define _MM_H_

#include <stddef.h>
#include <mm/paging.h>

#define PAGE_SIZE			0x1000

#define KHEAP_START			0x400000				/* kernel memory : from 0 to 16 MB */
#define KHEAP_SIZE			0xF00000
#define KMEM_SIZE			(KHEAP_START + KHEAP_SIZE)

#define UHEAP_START			0				 	/* user heap : just after text section */
#define UHEAP_END			0x40000000				/* can grow until UMAP_START */

#define UMAP_START			0x40000000				/* user memmory map : from 1 GB to 4 GB */
#define UMAP_END			0xF0000000

#define USTACK_START			0xF8000000				/* user stack : 3 KB at 4 GB */
#define USTACK_SIZE			0x8000

void init_mem(uint32_t start, uint32_t end);
void *kmalloc(uint32_t size);
void *kmalloc_align(uint32_t size);
void *kmalloc_align_phys(uint32_t size, uint32_t *phys);
void kfree(void *p);

#endif
