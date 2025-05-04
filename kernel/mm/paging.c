#include <mm/mm.h>
#include <proc/sched.h>
#include <mm/paging.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>

/* global pages */
uint32_t nr_pages;
struct page *page_array;

/* page directories */
pgd_t *pgd_kernel = NULL;

/* copy phsyical page (defined in x86/paging.s) */
extern void copy_page_physical(uint32_t src, uint32_t dst);

/*
 * Flush a Translation Lookaside Buffer entry.
 */
static inline void flush_tlb(uint32_t address)
{
	__asm__ __volatile__("invlpg (%0)" :: "r" (address) : "memory");
}

/*
 * Get or create a page table entry from pgd at virtual address.
 */
static pte_t *get_pte(uint32_t address, int make, pgd_t *pgd)
{
	uint32_t page_nr;
	void *tmp;
	pmd_t *pmd;

	/* get page table */
	pgd = pgd_offset(pgd, address);
	pmd = pmd_offset(pgd);
	page_nr = (address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1);

	/* page table already assigned */
	if (*pmd)
		return (pte_t *) pmd_page(*pmd) + page_nr;

	/* create a new page table */
	if (make) {
		/* allocate a new page table */
		tmp = get_free_page(GFP_KERNEL);
		if (!tmp)
			return NULL;

		/* memzero page table */
		memset(tmp, 0, PAGE_SIZE);

		/* set page table */
		*pmd = (pmd_t) __pa(tmp) | PAGE_TABLE;

		return (pte_t *) pmd_page(*pmd) + page_nr;
	}

	return 0;
}

/*
 * Remap pages to physical address.
 */
int remap_page_range(uint32_t start, uint32_t phys_addr, size_t size, pgd_t *pgd, int pgprot)
{
	uint32_t address;
	pte_t *pte;

	for (address = start; address < start + size; address += PAGE_SIZE, phys_addr += PAGE_SIZE) {
		/* get page table entry */
		pte = get_pte(address, 1, pgd);
		if (!pte)
			return -ENOMEM;

		/* set page table entry */
		*pte = mk_pte(phys_addr / PAGE_SIZE, pgprot);
	}

	return 0;
}

/*
 * Unmap a page.
 */
static void unmap_page(uint32_t address, pgd_t *pgd)
{
	uint32_t page_idx, page_nr;
	pmd_t *pmd;
	pte_t *pte;

	/* get page table */
	pgd = pgd_offset(pgd, address);
	pmd = pmd_offset(pgd);
	page_idx = (address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1);

	/* no matching page table */
	if (!*pmd)
		return;

	/* get page */
	pte = (pte_t *) pmd_page(*pmd) + page_idx;
	if (!pte)
		return;

	/* free page */
	page_nr = MAP_NR(pte_page(*pte));
	if (page_nr && page_nr < nr_pages)
		__free_page(&page_array[page_nr]);

	/* clear page table entry */
	pte_clear(pte);

	/* flush tlb */
	flush_tlb(address);
}

/*
 * Unmap pages.
 */
void unmap_pages(uint32_t start_address, uint32_t end_address, pgd_t *pgd)
{
	uint32_t address;

	/* unmap all pages */
	for (address = start_address; address < end_address; address += PAGE_SIZE)
		unmap_page(address, pgd);
}

/*
 * Anonymous page mapping.
 */
static int do_anonymous_page(struct vm_area *vma, pte_t *pte, void *address, int write_access)
{
	struct page *page;

	/* try to get a page */
	page = __get_free_page(GFP_USER);
	if (!page)
		return -ENOMEM;

	/* make page table entry */
	*pte = mk_pte(page->page, vma->vm_page_prot);
	if (write_access)
		*pte = pte_mkdirty(pte_mkwrite(*pte));

	/* memzero page */
	memset(address, 0, PAGE_SIZE);

	return 0;
}

/*
 * Handle a no page fault.
 */
static int do_no_page(struct task *task, struct vm_area *vma, uint32_t address, int write_access)
{
	struct page *page;
	pte_t *pte;

	/* get page table entry */
	pte = get_pte(address, 1, task->mm->pgd);
	if (!pte)
		return -ENOMEM;

	/* page table entry already set */
	if (!pte_none(*pte))
		return -EPERM;

	/* anonymous page mapping */
	if (!vma->vm_ops || !vma->vm_ops->nopage)
		return do_anonymous_page(vma, pte, (void *) PAGE_ALIGN_DOWN(address), write_access);

	/* specific mapping */
	page = vma->vm_ops->nopage(vma, address);
	if (!page)
		return -ENOSPC;

	/* make page table entry */
	*pte = mk_pte(page->page, vma->vm_page_prot);
	if (write_access)
		*pte = pte_mkdirty(pte_mkwrite(*pte));

	return 0;
}

/*
 * Handle a read only page fault.
 */
static int do_wp_page(struct task *task, uint32_t address)
{
	pte_t *pte;

	/* get page table entry */
	pte = get_pte(address, 0, task->mm->pgd);
	if (!pte)
		return -EINVAL;

	/* make page table entry writable */
	*pte = pte_mkdirty(pte_mkwrite(*pte));

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

	/* present page : try to make it writable */
	if (present) {
		ret = do_wp_page(current_task, fault_addr);
		if (ret)
			goto bad_area;
		else
			return;
	}

	/* non present page */
	ret = do_no_page(current_task, vma, fault_addr, write_access);
	if (ret)
		goto bad_area;

	return;
bad_area:
	/* output message */
	printf("Page fault at address=%x | present=%d write-access=%d user-mode=%d reserved=%d instruction-fetch=%d (process %d at %x)\n",
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
	pgd_new = (pgd_t *) get_free_page(GFP_KERNEL);
	if (!pgd_new)
		return NULL;

	/* copy kernel page directory */
	memcpy(pgd_new, pgd_kernel, PAGE_SIZE);

	return pgd_new;
}

/*
 * Allocate a new page table entry.
 */
static pte_t *pte_alloc(pmd_t *pmd, uint32_t offset)
{
	pte_t *pte;

	/* page table already allocated */
	if (!pmd_none(*pmd))
		goto out;

	/* create a new page table */
	pte = (pte_t *) get_free_page(GFP_KERNEL);
	if (!pte)
		return NULL;

	/* reset page table */
	memset(pte, 0, PAGE_SIZE);

	/* set page table */
	*pmd = __pa(pte) | PAGE_TABLE;

out:
	return (pte_t *) pmd_page(*pmd) + offset;
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
	uint32_t page_nr;

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

				/* read only or shared : share page */
				if ((vma->vm_flags & VM_SHARED) || !(vma->vm_flags & VM_WRITE)) {
					/* update page reference count */
					page_nr = MAP_NR(pte_page(pte));
					if (page_nr < nr_pages)
						page_array[page_nr].count++;

					/* set pte */
					*pte_dst = pte;
					goto next_pte;
				}

				/* try to get a page */
				page = __get_free_page(GFP_USER);
				if (!page)
					goto nomem;

				/* make page table entry */
				*pte_dst = mk_pte(page->page, pte_prot(pte));

				/* copy physical page */
				copy_page_physical(MAP_NR(pte_page(pte)) * PAGE_SIZE, MAP_NR(pte_page(*pte_dst)) * PAGE_SIZE);
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
	uint32_t page_nr;
	pte_t *ptes;
	int i;

	/* get table entries */
	ptes = (pte_t *) pmd_page(*pmd);

	/* free pages */
	for (i = 0; i < PTRS_PER_PTE; i++) {
		page_nr = MAP_NR(pte_page(ptes[i]));
		if (page_nr && page_nr < nr_pages)
			__free_page(&page_array[page_nr]);
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
int init_paging(uint32_t start, uint32_t end)
{
	uint32_t addr, i;
	pmd_t *pmd;
	pte_t *pte;

	/* compute number of pages */
	nr_pages = end / PAGE_SIZE;

	/* allocate kernel page directory after kernel code */
	pgd_kernel = (pgd_t *) PAGE_ALIGN_UP(start);
	memset(pgd_kernel, 0, PAGE_SIZE);
	start = (uint32_t) pgd_kernel + PAGE_SIZE;

	/* map kernel code pages to low memory */
	pmd = pmd_offset(pgd_kernel);
	for (addr = 0; addr < KHEAP_START + KHEAP_SIZE; ) {
		/* allocate page table */
		*pmd = (pmd_t) start | PAGE_TABLE;
		start += PAGE_SIZE;

		/* set page table entries */
		for (i = 0; i < PTRS_PER_PTE; i++) {
			pte = (pte_t *) (*pmd & PAGE_MASK) + i;

			if (addr < KHEAP_START + KHEAP_SIZE)
				*pte = mk_pte(addr / PAGE_SIZE, PAGE_READONLY);
			else
				*pte = 0;

			addr += PAGE_SIZE;
		}

		/* go to next page table */
		pmd++;
	}

	/* map kernel pages to high memory */
	pmd = pmd_offset(pgd_kernel) + 768;
	for (addr = 0; addr < end && addr < __pa(KPAGE_END);) {
		/* allocate page table */
		*pmd = (pmd_t) start | PAGE_TABLE;
		start += PAGE_SIZE;

		/* set page table entries */
		for (i = 0; i < PTRS_PER_PTE; i++) {
			pte = (pte_t *) (*pmd & PAGE_MASK) + i;

			if (addr < end && addr < __pa(KPAGE_END))
				*pte = mk_pte(addr / PAGE_SIZE, PAGE_KERNEL);
			else
				*pte = 0;

			addr += PAGE_SIZE;
		}

		/* go to next page table */
		pmd++;
	}

	/* register page fault handler */
	register_interrupt_handler(14, page_fault_handler);

	/* move kernel's pgd to high memory and enable paging */
	pgd_kernel = __va(pgd_kernel);
	switch_pgd(pgd_kernel);

	/* init page allocation */
	init_page_alloc(KHEAP_START + KHEAP_SIZE);

	/* init page cache */
	init_page_cache();

	return 0;
}
