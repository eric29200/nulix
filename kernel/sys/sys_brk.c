#include <sys/syscall.h>
#include <proc/sched.h>
#include <mm/mm.h>

/*
 * Change data segment end address.
 */
uint32_t sys_brk(uint32_t addr)
{
	uint32_t current_brk = current_task->end_brk;

	/* current brk is asked */
	if (addr < current_brk)
		return current_brk;

	/* check heap overflow */
	if (addr >= UMAP_START)
		return current_brk;

	/* mmap new region of brk */
	current_task->end_brk = addr;
	do_mmap(PAGE_ALIGN_UP(current_brk), PAGE_ALIGN_UP(current_task->end_brk) - PAGE_ALIGN_UP(current_brk), MAP_FIXED, NULL, 0);

	return current_task->end_brk;
}
