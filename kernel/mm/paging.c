#include <mm/mm.h>
#include <proc/sched.h>
#include <drivers/block/blk_dev.h>
#include <mm/paging.h>
#include <mm/highmem.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>

/* global pages */
uint32_t nr_pages;
struct page *page_array;

/* page directories */
pgd_t *pgd_kernel = NULL;

/*
 * Wait on a page.
 */
void wait_on_page(struct page *page)
{
	if (!PageLocked(page))
		return;

	execute_block_requests();

	if (PageLocked(page))
		panic("wait_on_page: page still locked after execute_block_requests()");
}

/*
 * Flush a Translation Lookaside Buffer entry.
 */
void flush_tlb_page(pgd_t *pgd, uint32_t address)
{
	if (pgd == current_task->mm->pgd)
		__asm__ __volatile__("invlpg (%0)" :: "r" (address) : "memory");
}

/*
 * Flush Translation Lookaside Buffers.
 */
void flush_tlb(pgd_t *pgd)
{
	if (pgd == current_task->mm->pgd)
		switch_pgd(pgd);
}

/*
 * Allocate a new page directory entry.
 */
static inline pmd_t *pmd_alloc(pgd_t *pgd, uint32_t address)
{
	UNUSED(address);
	return (pmd_t *) pgd;
}

/*
 * Allocate a new page table entry.
 */
static pte_t *pte_alloc(pmd_t *pmd, uint32_t address)
{
	uint32_t offset = address = (address >> (PAGE_SHIFT - 2)) & 4 * (PTRS_PER_PTE - 1);
	pte_t *pte;

	/* page table already allocated */
	if (!pmd_none(*pmd))
		goto out;

	/* create a new page table */
	pte = (pte_t *) get_free_page();
	if (!pte)
		return NULL;

	/* reset page table */
	memset(pte, 0, PAGE_SIZE);

	/* set page table */
	*pmd = __pa(pte) | PAGE_TABLE;
out:
	return (pte_t *) (pmd_page(*pmd) + offset);
}

/*
 * Free a page table entry.
 */
static int pte_free(pte_t *pte)
{
	struct page *page;

	if (pte_none(*pte) || !pte_present(*pte))
		return 0;

	/* get page */
	page = pte_page(*pte);
	if (!VALID_PAGE(page))
		return 0;

	/* free page */
	__free_page(page);
	return 1;
}

/*
 * Forget a page table entry.
 */
static void forget_pte(pte_t pte)
{
	struct page *page;

	if (pte_none(pte))
		return;

	/* get page */
	page = pte_page(pte);
	if (!VALID_PAGE(page))
		return;

	/* free page */
	__free_page(page);
}

/*
 * Remap pages.
 */
static void remap_pte_range(pte_t *pte, uint32_t start, size_t size, uint32_t phys_addr, int pgprot)
{
	pte_t old_page;
	uint32_t end;

	/* compute end address */
	end = start + size;
	if (end > ((start + PMD_SIZE) & PMD_MASK))
		end = ((start + PMD_SIZE) & PMD_MASK);

	/* remap page table entries */
	do {
		old_page = *pte;

		/* set page table entry */
		pte_clear(pte);
		*pte = mk_pte_phys(phys_addr, pgprot);

		/* forget old entry */
		if (!pte_none(old_page))
			forget_pte(old_page);

		start += PAGE_SIZE;
		phys_addr += PAGE_SIZE;
		pte++;
	} while (start < end);
}

/*
 * Remap pages.
 */
int remap_pmd_range(pmd_t *pmd, uint32_t start, size_t size, uint32_t phys_addr, int pgprot)
{
	uint32_t end;
	pte_t *pte;

	phys_addr -= start;

	/* compute end address */
	end = start + size;
	if (end > ((start + PGDIR_SIZE) & PGDIR_MASK))
		end = ((start + PGDIR_SIZE) & PGDIR_MASK);

	/* free page table */
	do {
		/* allocate page table entries */
		pte = pte_alloc(pmd, start);
		if (!pte)
			return -ENOMEM;

		/* remap */
		remap_pte_range(pte, start, end - start, phys_addr + start, pgprot);

		start += (start + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (start < end);

	return 0;
}

/*
 * Remap pages.
 */
int remap_page_range(uint32_t start, uint32_t phys_addr, size_t size, int pgprot)
{
	uint32_t end = start + size;
	int ret = 0;
	pmd_t *pmd;
	pgd_t *dir;

	/* get page directory */
	dir = pgd_offset(current_task->mm->pgd, start);

	/* fix physical address */
	phys_addr -= start;

	/* remap page directory */
	do {
		/* allocate page directory */
		pmd = pmd_alloc(dir, start);
		if (!pmd) {
			ret = -ENOMEM;
			break;
		}

		/* remap */
		ret = remap_pmd_range((pmd_t *) dir, start, end - start, phys_addr + start, pgprot);
		if (ret)
			break;

		start = (start + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (start < end);

	/* flush tlb */
	flush_tlb(current_task->mm->pgd);

	return ret;
}

/*
 * Unmap pages.
 */
static size_t zap_pte_range(pmd_t *pmd, uint32_t address, size_t size)
{
	uint32_t page_idx, end;
	size_t freed = 0;
	pte_t *pte;

	/* check page table */
	if (pmd_none(*pmd))
		return 0;

	/* compute end address */
	end = address + size;
	if (end > ((address + PMD_SIZE) & PMD_MASK))
		end = ((address + PMD_SIZE) & PMD_MASK);

	/* free page table entries */
	do {
		/* get page table entry */
		page_idx = (address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1);
		pte = (pte_t *) pmd_page(*pmd) + page_idx;

		/* free page table entry */
		if (!pte_none(*pte)) {
			freed += pte_free(pte);
			pte_clear(pte);
		}

		address += PAGE_SIZE;
	} while (address < end);

	return freed;
}

/*
 * Unmap pages.
 */
static size_t zap_pmd_range(pgd_t *dir, uint32_t address, size_t size)
{
	size_t freed = 0;
	uint32_t end;
	pmd_t *pmd;

	/* get page table */
	pmd = pmd_offset(dir);

	/* compute end address */
	end = address + size;
	if (end > ((address + PGDIR_SIZE) & PGDIR_MASK))
		end = ((address + PGDIR_SIZE) & PGDIR_MASK);

	/* free page table */
	do {
		freed += zap_pte_range(pmd, address, end - address);
		address += (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);

	return freed;
}

/*
 * Unmap pages.
 */
size_t zap_page_range(pgd_t *pgd, uint32_t address, uint32_t size)
{
	uint32_t end = address + size;
	size_t freed = 0;
	pgd_t *dir;

	/* get page directory */
	dir = pgd_offset(pgd, address);

	/* free page directory */
	do {
		freed += zap_pmd_range(dir, address, end - address);
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (address < end);

	/* flush tlb */
	flush_tlb(pgd);

	return freed;
}

/*
 * Anonymous page mapping.
 */
static int do_anonymous_page(struct vm_area *vma, pte_t *pte, int write_access)
{
	struct page *page;

	/* try to get a page */
	page = __get_free_page(GFP_HIGHUSER);
	if (!page)
		return -ENOMEM;

	/* clear page */
	clear_user_highpage(page);

	/* make page table entry */
	*pte = mk_pte(page, vma->vm_page_prot);
	if (write_access)
		*pte = pte_mkdirty(pte_mkwrite(*pte));

	/* update memory size */
	vma->vm_mm->rss++;

	return 0;
}

/*
 * Handle a no page fault.
 */
static int do_no_page(struct vm_area *vma, uint32_t address, int write_access, pte_t *pte)
{
	struct page *page, *new_page;

	/* page align address */
	address &= PAGE_MASK;

	/* anonymous page mapping */
	if (!vma->vm_ops || !vma->vm_ops->nopage)
		return do_anonymous_page(vma, pte, write_access);

	/* specific mapping */
	new_page = vma->vm_ops->nopage(vma, address);
	if (!new_page)
		return -ENOMEM;

	/* no share : copy to new page and keep old page in offset */
	if (write_access && !(vma->vm_flags & VM_SHARED)) {
		/* get a new page */
		page = __get_free_page(GFP_HIGHUSER);
		if (!page) {
			__free_page(new_page);
			return -ENOMEM;
		}

		/* copy page */
		copy_user_highpage(page, new_page);

		/* release cache page */
		__free_page(new_page);
		new_page = page;
	}

	/* make page table entry */
	*pte = mk_pte(new_page, vma->vm_page_prot);
	if (write_access)
		*pte = pte_mkdirty(pte_mkwrite(*pte));

	/* update memory size */
	vma->vm_mm->rss++;

	return 0;
}

/*
 * Handle a read only page fault.
 */
static int do_wp_page(struct vm_area *vma, uint32_t address, pte_t *pte)
{
	struct page *old_page, *new_page;

	/* get page */
	old_page = pte_page(*pte);

	/* only one user make page table entry writable */
	if (old_page->count == 1) {
		*pte = pte_mkdirty(pte_mkwrite(*pte));
		flush_tlb_page(vma->vm_mm->pgd, address);
		return 0;
	}

	/* get a new page */
	new_page = __get_free_page(GFP_HIGHUSER);
	if (!new_page)
		return -ENOMEM;

	/* copy page */
	copy_user_highpage(new_page, old_page);

	/* set pte */
	*pte = pte_mkdirty(pte_mkwrite(mk_pte(new_page, vma->vm_page_prot)));
	flush_tlb_page(vma->vm_mm->pgd, address);

	/* free old page */
	__free_page(old_page);

	/* update memory size */
	vma->vm_mm->rss++;

	return 0;
}

/*
 * Expand stack.
 */
static int expand_stack(struct vm_area *vma, uint32_t addr)
{
	/* page align address */
	addr &= PAGE_MASK;

	/* check stack limit */
	if (vma->vm_end - addr > USTACK_LIMIT)
		return -ENOMEM;

	/* grow stack */
	vma->vm_start = addr;

	return 0;
}

/*
 * Handle memory fault.
 */
static int handle_pte_fault(struct vm_area *vma, uint32_t address, int write_access, pte_t *pte)
{
	/* page not present */
	if (!pte_present(*pte))
		return do_no_page(vma, address, write_access, pte);

	/* write access */
	if (write_access)
		return do_wp_page(vma, address, pte);

	return 0;
}

/*
 * Handle memory fault.
 */
static int handle_mm_fault(struct vm_area *vma, uint32_t address, int write_access)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	/* get page table entry */
	pgd = pgd_offset(vma->vm_mm->pgd, address);
	pmd = pmd_alloc(pgd, address);
	if (!pmd)
		return -ENOMEM;
	pte = pte_alloc(pmd, address);
	if (!pte)
		return -ENOMEM;

	/* handle fault */
	return handle_pte_fault(vma, address, write_access, pte);
}

/*
 * Page fault handler.
 */
void page_fault_handler(struct registers *regs)
{
	int present, write_access, user, reserved, id, ret;
	struct vm_area *vma;
	uint32_t fault_addr;

	/* faulting address is stored in CR2 register */
	__asm__ volatile("mov %%cr2, %0" : "=r" (fault_addr));

	/* page fault on task end : kill current task */
	if (fault_addr == TASK_RETURN_ADDRESS) {
		sys_exit(0);
		return;
	}

	/* get fault informations */
	present = regs->err_code & 0x1 ? 1 : 0;
	write_access = regs->err_code & 0x2 ? 1 : 0;
	user = regs->err_code & 0x4 ? 1 : 0;
	reserved = regs->err_code & 0x8 ? 1 : 0;
	id = regs->err_code & 0x10 ? 1 : 0;

	/* get memory region */
	vma = find_vma(current_task, fault_addr);
	if (vma)
		goto good_area;

	/* maybe stack needs to be grown ? */
	vma = find_vma_next(current_task, fault_addr);
	if (vma && (vma->vm_flags & VM_GROWSDOWN))
		goto expand_stack;

	/* else bad page fault */
	goto bad_area;
expand_stack:
	if (expand_stack(vma, fault_addr))
		goto bad_area;
	return;
good_area:
	/* write violation */
	if (write_access && !(vma->vm_flags & VM_WRITE))
		goto bad_area;

	/* handle fault */
	ret = handle_mm_fault(vma, fault_addr, write_access);
	if (ret)
		goto bad_area;

	return;
bad_area:
	/* output message */
	printf("Page fault at address=0x%x | present=%d write-access=%d user-mode=%d reserved=%d instruction-fetch=%d (process %d at 0x%x)\n",
	       fault_addr, present, write_access, user, reserved, id, current_task->pid, regs->eip);

	/* user mode : exit process */
	if (user)
		sys_exit(1);

	/* otherwise panic */
	panic("");
}

/*
 * Switch page directory.
 */
void switch_pgd(pgd_t *pgd)
{
	uint32_t cr0;

	/* switch */
	__asm__ volatile("mov %0, %%cr3" :: "r" (__pa(pgd)));
	__asm__ volatile("mov %%cr0, %0" : "=r" (cr0));

	/* enable paging */
	cr0 |= 0x80000000;
	__asm__ volatile("mov %0, %%cr0" :: "r" (cr0));
}

/*
 * Create a new page directory.
 */
pgd_t *create_page_directory()
{
	pgd_t *pgd_new;

	/* create a new page directory */
	pgd_new = (pgd_t *) get_free_page();
	if (!pgd_new)
		return NULL;

	/* copy kernel page directory */
	memcpy(pgd_new, pgd_kernel, PAGE_SIZE);

	return pgd_new;
}

/*
 * Copy page range.
 */
int copy_page_range(pgd_t *pgd_src, pgd_t *pgd_dst, struct vm_area *vma)
{
	uint32_t address = vma->vm_start, end = vma->vm_end;
	pte_t *pte_src, *pte_dst, pte;
	pmd_t *pmd_src, *pmd_dst;
	struct page *page;
	int cow;

	/* copy on write ? */
	cow = (vma->vm_flags & (VM_SHARED | VM_WRITE)) == VM_WRITE;

	pgd_src = pgd_offset(pgd_src, address) - 1;
	pgd_dst = pgd_offset(pgd_dst, address) - 1;

	for (;;) {
		pgd_src++;
		pgd_dst++;

		pmd_src = pmd_offset(pgd_src);
		pmd_dst = pmd_offset(pgd_dst);

		/* for each page table */
		do {
			if (pmd_none(*pmd_src)) {
				address = (address + PMD_SIZE) & PMD_MASK;
				if (address >= end)
					goto out;
			}

			/* allocate a new pmd */
			if (pmd_none(*pmd_dst))
				if (!pte_alloc(pmd_dst, 0))
					goto nomem;

			pte_src = pte_offset(pmd_src, address);
			pte_dst = pte_offset(pmd_dst, address);

			/* for each page table entry */
			do {
				pte = *pte_src;

				/* skip empty */
				if (pte_none(pte))
					goto next_pte;

				/* virtual page */
				page = pte_page(pte);
				if (!VALID_PAGE(page)) {
					*pte_dst = pte;
					goto next_pte;
				}

				/* copy on write : write protect it both in src and dst */
				if (cow) {
					pte = pte_wrprotect(pte);
					*pte_src = pte;
				}

				/* shared mapping : mark it clean in dst */
				if (vma->vm_flags & VM_SHARED)
					pte = pte_mkclean(pte);

				/* set pte */
				*pte_dst = pte_mkold(pte);

				/* update page reference count */
				page->count++;
next_pte:
				/* go to next page table entry */
				address += PAGE_SIZE;
				if (address >= end)
					goto out;

				pte_src++;
				pte_dst++;
			} while ((uint32_t) pte_src & PTE_TABLE_MASK);

			pmd_src++;
			pmd_dst++;
		} while ((uint32_t) pmd_src & PMD_TABLE_MASK);
	}

out:
	return 0;
nomem:
	return -ENOMEM;
}

/*
 * Free a page table.
 */
static void free_pmd(pmd_t *pmd)
{
	struct page *page;
	pte_t *ptes;
	int i;

	/* get table entries */
	ptes = (pte_t *) pmd_page(*pmd);

	/* free pages */
	for (i = 0; i < PTRS_PER_PTE; i++) {
		if (pte_none(ptes[i]))
			continue;

		page = pte_page(ptes[i]);
		if (VALID_PAGE(page))
			__free_page(page);
	}

	/* free page table */
	free_page(ptes);
}

/*
 * Free a page directory.
 */
void free_pgd(pgd_t *pgd)
{
	pmd_t *pmd, *pmd_kernel;
	int i;

	if (!pgd)
		return;

	/* get page tables */
	pmd = pmd_offset(pgd);
	pmd_kernel = pmd_offset(pgd_kernel);

	/* free page tables */
	for (i = 0; i < PTRS_PER_PTE; i++, pmd++, pmd_kernel++)
		if (pmd_page(*pmd) != pmd_page(*pmd_kernel))
			free_pmd(pmd);

	/* free page directory */
	free_page(pgd);
}

/*
 * Init paging.
 */
int init_paging(uint32_t kernel_start, uint32_t kernel_end, uint32_t mem_end)
{
	uint32_t addr, i;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	int ret;

	/* compute number of pages */
	nr_pages = mem_end / PAGE_SIZE;

	/* allocate kernel page directory after kernel code */
	pgd_kernel = (pgd_t *) PAGE_ALIGN_UP(kernel_end);
	memset(pgd_kernel, 0, PAGE_SIZE);
	kernel_end = (uint32_t) pgd_kernel + PAGE_SIZE;

	/* map kernel code pages to low memory */
	pmd = pmd_offset(pgd_kernel);
	for (addr = KCODE_START; addr < KCODE_END; ) {
		/* allocate page table */
		*pmd = (pmd_t) kernel_end | PAGE_TABLE;
		kernel_end += PAGE_SIZE;

		/* set page table entries */
		for (i = 0; i < PTRS_PER_PTE; i++) {
			pte = (pte_t *) (*pmd & PAGE_MASK) + i;
			*pte = mk_pte_phys(addr, PAGE_READONLY);
			addr += PAGE_SIZE;
		}

		/* go to next page table */
		pmd++;
	}

	/* map kernel pages to high memory */
	pmd = pmd_offset(pgd_kernel) + 768;
	for (addr = 0; addr < mem_end && addr < __pa(KPAGE_END);) {
		/* allocate page table */
		*pmd = (pmd_t) kernel_end | PAGE_TABLE;
		kernel_end += PAGE_SIZE;

		/* set page table entries */
		for (i = 0; i < PTRS_PER_PTE; i++) {
			pte = (pte_t *) (*pmd & PAGE_MASK) + i;

			if (addr < mem_end && addr < __pa(KPAGE_END))
				*pte = mk_pte_phys(addr, PAGE_KERNEL);
			else
				*pte = 0;

			addr += PAGE_SIZE;
		}

		/* go to next page table */
		pmd++;
	}

	/* allocate pkmap page table entries */
	addr = PKMAP_BASE;
	pgd = pgd_offset(pgd_kernel, addr);
	pmd = pmd_offset(pgd);
	*pmd = (pmd_t) kernel_end | PAGE_TABLE;
	kernel_end += PAGE_SIZE;
	pkmap_page_table = pte_offset(pmd, addr);

	/* register page fault handler */
	register_interrupt_handler(14, page_fault_handler);

	/* move kernel's pgd to high memory and enable paging */
	pgd_kernel = __va(pgd_kernel);
	switch_pgd(pgd_kernel);

	/* init page allocation */
	ret = init_page_alloc(kernel_start, kernel_end);
	if (ret)
		return ret;

	/* init page cache */
	init_page_cache();

	return 0;
}
