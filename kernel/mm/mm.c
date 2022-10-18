#include <mm/mm.h>
#include <mm/paging.h>
#include <mm/heap.h>
#include <fs/fs.h>
#include <string.h>
#include <stdio.h>

/* placement address (used before kernel heap is created) */
uint32_t placement_address = 0;

/* kernel heap */
struct heap_t *kheap = NULL;

/*
 * Allocate memory (internal function).
 */
static void *__kmalloc(uint32_t size, uint8_t align, uint32_t *phys)
{
	struct page_t *page;
	void *ret;

	/* use kernel heap */
	if (kheap) {
		ret = heap_alloc(kheap, size, align);
		if (!ret)
			return NULL;

		if (phys) {
			page = get_page((uint32_t) ret, 0, kernel_pgd);
			*phys = page->frame * PAGE_SIZE + ((uint32_t) ret & 0xFFF);
		}

		return ret;
	}

	/* align adress on PAGE boundary */
	if (align == 1 && !PAGE_ALIGNED(placement_address))
		placement_address = PAGE_ALIGN_UP(placement_address);

	/* get physical memory */
	if (phys)
		*phys = placement_address;

	/* update placement_address */
	ret = (void *) placement_address;
	placement_address += size;
	return ret;
}

/*
 * Allocate memory.
 */
void *kmalloc(uint32_t size)
{
	return __kmalloc(size, 0, NULL);
}

/*
 * Allocate page aligned memory.
 */
void *kmalloc_align(uint32_t size)
{
	return __kmalloc(size, 1, NULL);
}

/*
 * Allocate page aligned memory and output physical memory.
 */
void *kmalloc_align_phys(uint32_t size, uint32_t *phys)
{
	return __kmalloc(size, 1, phys);
}

/*
 * Free memory on the kernel heap.
 */
void kfree(void *p)
{
	if (kheap)
		heap_free(kheap, p);
}

/*
 * Init memory paging and kernel heap.
 */
void init_mem(uint32_t start, uint32_t end)
{
	int ret;

	/* set placement address */
	placement_address = start;

	/* init paging */
	ret = init_paging(start, end);
	if (ret)
		panic("Cannot init paging");

	/* init heap */
	kheap = heap_create(KHEAP_START, KHEAP_SIZE);
	if (!kheap)
		panic("Cannot create kernel heap");
}
