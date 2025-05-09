#include <mm/mm.h>
#include <mm/paging.h>
#include <fs/fs.h>
#include <string.h>
#include <stdio.h>

/*
 * Init memory paging and kernel heap.
 */
void init_mem(uint32_t kernel_start, uint32_t kernel_end, uint32_t mem_end)
{
	int ret;

	/* init paging */
	ret = init_paging(kernel_start, kernel_end, mem_end);
	if (ret)
		panic("Cannot init paging");

	/* init heap */
	kheap_init();
}
