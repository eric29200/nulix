#include <proc/sched.h>
#include <mm/swap.h>
#include <stdio.h>

/*
 * Try to swap out a page.
 */
static int try_to_swap_out(struct vm_area *vma, uint32_t address, pte_t *pte)
{
	struct page *page;

	/* page not present */
	if (!pte_present(*pte))
		return 0;

	/* non valid page */
	page = pte_page(*pte);
	if (!VALID_PAGE(page))
		return 0;

	if (!page->inode || PageReserved(page))
		return 0;

	/* clean page : drop pte */
	if (!pte_dirty(*pte))
		goto drop_pte;

	/* swapout not implemented */
	if (!vma->vm_ops || !vma->vm_ops->swapout)
		return 0;

	/* swap out page */
	vma->vm_ops->swapout(vma, page);
drop_pte:
	/* drop pte and free page */
	pte_clear(pte);
	vma->vm_mm->rss--;
	flush_tlb_page(vma->vm_mm->pgd, address);
	__free_page(page);
	return 1;
}

/*
 * Try to swap out a page table.
 */
static int swap_out_pmd(struct vm_area *vma, pmd_t *pmd, uint32_t address, uint32_t end)
{
	uint32_t pmd_end;
	pte_t *pte;
	int ret;

	if (pmd_none(*pmd))
		return 0;

	pte = pte_offset(pmd, address);

	pmd_end = (address + PMD_SIZE) & PMD_MASK;
	if (end > pmd_end)
		end = pmd_end;

	do {
		vma->vm_mm->swap_address = address + PAGE_SIZE;

		ret -= try_to_swap_out(vma, address, pte);
		if (ret)
			return ret;

		address += PAGE_SIZE;
		pte++;
	} while (address < end);

	return 0;
}

/*
 * Try to swap out a page directory.
 */
static int swap_out_pgd(struct vm_area *vma, pgd_t *pgd, uint32_t address, uint32_t end)
{
	uint32_t pgd_end;
	pmd_t *pmd;
	int ret;

	pmd = pmd_offset(pgd);

	pgd_end = (address + PGDIR_SIZE) & PGDIR_MASK;
	if (end > pgd_end)
		end = pgd_end;

	do {
		ret = swap_out_pmd(vma, pmd, address, end);
		if (ret)
			return ret;

		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);

	return 0;
}

/*
 * Try to swap out a memory region.
 */
static int swap_out_vma(struct vm_area *vma, uint32_t address)
{
	uint32_t end;
	pgd_t *pgd;
	int ret;

	/* don't try to swap out locked regions */
	if (vma->vm_flags & VM_LOCKED)
		return 0;

	pgd = pgd_offset(vma->vm_mm->pgd, address);

	for (end = vma->vm_end; address < end;) {
		ret = swap_out_pgd(vma, pgd, address, end);
		if (ret)
			return ret;

		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		pgd++;
	}

	return 0;
}

/*
 * Try to swap out a process.
 */
static int swap_out_process(struct task *task)
{
	struct vm_area *vma;
	uint32_t address;
	int ret;

	/* start at swap address */
	address = task->mm->swap_address;

	/* find memory region to swap */
	vma = find_vma(task, address);
	if (!vma)
		goto out;

	if (address < vma->vm_start)
		address = vma->vm_start;

	for (;;) {
		/* try to swap out memory region */
		ret = swap_out_vma(vma, address);
		if (ret)
			return ret;

		/* try next memory region */
		vma = list_next_entry_or_null(vma, &task->mm->vm_list, list);
		if (!vma)
			break;

		address = vma->vm_start;
	}

out:
	task->mm->swap_address = 0;
	task->mm->swap_cnt = 0;
	return 0;
}

/*
 * Find best task to swap out.
 */
static struct task *__find_best_task_to_swap_out(int assign)
{
	struct task *task, *best = NULL;
	struct list_head *pos;
	uint32_t max_cnt = 0;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);

		if (task->mm->rss <= 0)
			continue;

		/* assign swap counter ? */
		if (assign == 1)
			task->mm->swap_cnt = task->mm->rss;

		/* find task with max memory */
		if (task->mm->swap_cnt > max_cnt) {
			max_cnt = task->mm->swap_cnt;
			best = task;
		}
	}

	return best;
}

/*
 * Try to swap out some process.
 */
int swap_out(int priority)
{
	struct task *task;
	int count, ret;

	/* number of tasks to scan */
	count = nr_tasks / priority;
	if (count < 1)
		count = 1;

	for (; count >= 0; count--) {
		/* find best task to swap out */
		task = __find_best_task_to_swap_out(0);
		if (!task) {
			/* reassign swap counter */
			task = __find_best_task_to_swap_out(1);
			if (!task)
				return 0;
		}

		/* try to swap out process */
		ret = swap_out_process(task);
		if (ret)
			return ret;
	}

	return 0;
}
