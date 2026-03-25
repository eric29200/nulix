#include <sys/sys.h>
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
		panic("Cannot init paging\n");

	/* init heap */
	kheap_init();
}

/*
 * Get informations on memory.
 */
void si_meminfo(struct sysinfo *info)
{
	info->totalram = totalram_pages << PAGE_SHIFT;
	info->freeram = nr_free_pages() << PAGE_SHIFT;
	info->bufferram = buffermem_pages << PAGE_SHIFT;
}