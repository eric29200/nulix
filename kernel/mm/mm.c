#include <mm/mm.h>
#include <mm/paging.h>
#include <mm/kheap.h>
#include <fs/fs.h>
#include <string.h>
#include <stdio.h>

/*
 * Allocate memory (internal function).
 */
static void *__kmalloc(uint32_t size, uint8_t align)
{
	return kheap_alloc(size, align);
}

/*
 * Allocate memory.
 */
void *kmalloc(uint32_t size)
{
	return __kmalloc(size, 0);
}

/*
 * Allocate page aligned memory.
 */
void *kmalloc_align(uint32_t size)
{
	return __kmalloc(size, 1);
}

/*
 * Free memory on the kernel heap.
 */
void kfree(void *p)
{
	kheap_free(p);
}

/*
 * Init memory paging and kernel heap.
 */
void init_mem(uint32_t end)
{
	int ret;

	/* init heap */
	kheap_init(KHEAP_START, KHEAP_SIZE);

	/* init paging */
	ret = init_paging(end);
	if (ret)
		panic("Cannot init paging");
}
