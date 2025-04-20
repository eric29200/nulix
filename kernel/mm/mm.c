#include <mm/mm.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <string.h>
#include <stdio.h>

/*
 * Init memory paging and kernel heap.
 */
void init_mem(uint32_t start, uint32_t end)
{
	int ret;

	/* init paging */
	ret = init_paging(start, end);
	if (ret)
		panic("Cannot init paging");

	/* init heap */
	kheap_init(KHEAP_START, KHEAP_SIZE);
}
