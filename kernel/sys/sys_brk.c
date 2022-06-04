#include <sys/syscall.h>
#include <proc/sched.h>
#include <mm/mm.h>

/*
 * Change data segment end address.
 */
uint32_t sys_brk(uint32_t addr)
{
	/* current brk is asked */
	if (addr < current_task->end_text)
		return current_task->end_brk;

	/* check heap overflow */
	if (addr >= UMAP_START)
		return current_task->end_brk;

	/* update brk */
	current_task->end_brk = addr;
	return current_task->end_brk;
}
