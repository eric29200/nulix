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

	if (!page->inode)
		return 0;

	/* page dirty */
	if (pte_dirty(*pte))
		return 0;

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
static int swap_out_pmd(struct vm_area *vma, pmd_t *pmd, uint32_t address, uint32_t end, int count)
{
	uint32_t pmd_end;
	pte_t *pte;

	if (pmd_none(*pmd))
		return 0;
	
	pte = pte_offset(pmd, address);
	
	pmd_end = (address + PMD_SIZE) & PMD_MASK;
	if (end > pmd_end)
		end = pmd_end;

	do {
		vma->vm_mm->swap_address = address + PAGE_SIZE;

		count -= try_to_swap_out(vma, address, pte);
		if (!count)
			break;

		address += PAGE_SIZE;
		pte++;
	} while (address < end);

	return count;
}

/*
 * Try to swap out a page directory.
 */
static int swap_out_pgd(struct vm_area *vma, pgd_t *pgd, uint32_t address, uint32_t end, int count)
{
	uint32_t pgd_end;
	pmd_t *pmd;

	pmd = pmd_offset(pgd);

	pgd_end = (address + PGDIR_SIZE) & PGDIR_MASK;	
	if (end > pgd_end)
		end = pgd_end;
	
	do {
		count = swap_out_pmd(vma, pmd, address, end, count);
		if (!count)
			break;

		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);

	return count;
}

/*
 * Try to swap out a memory region.
 */
static int swap_out_vma(struct vm_area *vma, uint32_t address, int count)
{
	uint32_t end;
	pgd_t *pgd;

	/* don't try to swap out locked regions */
	if (vma->vm_flags & VM_LOCKED)
		return 0;

	pgd = pgd_offset(vma->vm_mm->pgd, address);

	for (end = vma->vm_end; address < end;) {
		count = swap_out_pgd(vma, pgd, address, end, count);
		if (!count)
			break;

		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		pgd++;
	}

	return count;
}

/*
 * Try to swap out a process.
 */
static int swap_out_process(struct task *task, int count)
{
	struct vm_area *vma;
	uint32_t address;

	/* start at swap address */
	address = task->mm->swap_address;

	/* find memory region to swap */
	vma = find_vma(task, address);
	if (!vma) {
		vma = find_vma_next(task, address);
		if (!vma)
			goto out;
	}

	if (address < vma->vm_start)
		address = vma->vm_start;

	for (;;) {
		/* try to swap out memory region */
		count = swap_out_vma(vma, address, count);
		if (!count)
			return count;

		/* try next memory region */
		vma = list_next_entry_or_null(vma, &task->mm->vm_list, list);
		if (!vma)
			break;

		address = vma->vm_start;
	}

out:
	task->mm->swap_address = 0;
	return count;
}

/*
 * Try to swap out some process.
 */
int swap_out(int count)
{
	struct list_head *pos;
	struct task *task;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);

		/* try to swap out process */
		count = swap_out_process(task, count);
		if (!count)
			break;
	}

	return count;
}