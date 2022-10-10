#include <sys/syscall.h>
#include <proc/sched.h>
#include <mm/mm.h>

/*
 * Change data segment end address.
 */
uint32_t sys_brk(uint32_t brk)
{
	uint32_t newbrk, oldbrk;

	/* current brk is asked */
	if (brk < current_task->mm->end_text)
		goto out;

	/* grow brk, without new page */
	newbrk = PAGE_ALIGN_UP(brk);
	oldbrk = PAGE_ALIGN_UP(current_task->mm->end_brk);
	if (oldbrk == newbrk)
		goto set_brk;

	/* shrink brk */
	if (brk <= current_task->mm->end_brk) {
		if (do_munmap(newbrk, oldbrk - newbrk) == 0)
			goto set_brk;
		goto out;
	}

	/* check against existing mapping */
	if (find_vma_intersection(current_task, oldbrk, newbrk + PAGE_SIZE))
		goto out;

	/* map new pages */
	if (do_mmap(oldbrk, newbrk - oldbrk, MAP_FIXED, NULL, 0) != (void *) oldbrk)
		goto out;

set_brk:
	current_task->mm->end_brk = brk;
out:
	return current_task->mm->end_brk;
}
