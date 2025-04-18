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
		*pmd = (pmd_t) kmalloc_align(PAGE_SIZE) | PAGE_TABLE;
		if (!*pmd)
			return NULL;

		/* clear page table entries */
		memset((void *) pmd_page(*pmd), 0, PAGE_SIZE);

		/* flush tlb */
		flush_tlb(address);

		return (pte_t *) pmd_page(*pmd) + page_nr;
	}

	return 0;
}

/*
 * Set a page table entry.
 */
static int set_pte(pte_t *pte, int pgprot, struct page *page)
{
	/* check page */
	if (!page)
		return -EINVAL;

	/* page table entry already set */
	if (PTE_PAGE(*pte) != 0)
		return -EPERM;

	/* set page table entry */
	*pte = MK_PTE(page->page, pgprot);

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
		*pte = MK_PTE(phys_addr / PAGE_SIZE, pgprot);
	}

	return 0;
}

/*
 * Unmap a page.
 */
static void unmap_page(uint32_t address, pgd_t *pgd)
{
	uint32_t page_nr, page_idx;
	pmd_t *pmd;
	pte_t *pte;

	/* get page table */
	pgd = pgd_offset(pgd, address);
	pmd = pmd_offset(pgd);
	page_nr = (address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1);

	/* no matching page table */
	if (!*pmd)
		return;

	/* get page */
	pte = (pte_t *) pmd_page(*pmd) + page_nr;
	if (!pte)
		return;

	/* free page */
	page_idx = PTE_PAGE(*pte);
	if (page_idx && page_idx < nr_pages)
		__free_page(&page_array[page_idx]);

	/* reset page table entry */
	*pte = 0;

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
static int do_anonymous_page(struct vm_area *vma, pte_t *pte, void *address)
{
	struct page *page;
	int ret;

	/* try to get a page */
	page = __get_free_page(GFP_USER);
	if (!page)
		return -ENOMEM;

	/* set page table entry */
	ret = set_pte(pte, vma->vm_page_prot, page);
	if (ret)
		goto err;

	/* memzero page */
	memset(address, 0, PAGE_SIZE);

	return 0;
err:
	__free_page(page);
	return ret;
}

/*
 * Handle a no page fault.
 */
static int do_no_page(struct task *task, struct vm_area *vma, uint32_t address)
{
	struct page *page;
	pte_t *pte;

	/* get page table entry */
	pte = get_pte(address, 1, task->mm->pgd);
	if (!pte)
		return -ENOMEM;

	/* page table entry already set */
	if (PTE_PAGE(*pte) != 0)
		return -EPERM;

	/* anonymous page mapping */
	if (!vma->vm_ops || !vma->vm_ops->nopage)
		return do_anonymous_page(vma, pte, (void *) PAGE_ALIGN_DOWN(address));

	/* specific mapping */
	page = vma->vm_ops->nopage(vma, address);
	if (!page)
		return -ENOSPC;

	/* set page table entry */
	set_pte(pte, vma->vm_page_prot, page);

	return 0;
}

/*
 * Handle a read only page fault.
 */
static int do_wp_page(struct task *task, uint32_t address)
{
	struct page *page;
	uint32_t page_idx;
	pte_t *pte;

	/* get page table entry */
	pte = get_pte(address, 0, task->mm->pgd);
	if (!pte)
		return -EINVAL;

	/* get page */
	page_idx = PTE_PAGE(*pte);
	if (page_idx <= 0 || page_idx >= nr_pages)
		return -EINVAL;
	page = &page_array[page_idx];

	/* make page table entry writable */
	*pte = MK_PTE(page->page, PTE_PROT(*pte) | PAGE_RW | PAGE_DIRTY);

	return 0;
}

/*
 * Page fault handler.
 */
void page_fault_handler(struct registers *regs)
{
	struct vm_area *vma;
	uint32_t fault_addr;
	int ret;

	/* faulting address is stored in CR2 register */
	__asm__ volatile("mov %%cr2, %0" : "=r" (fault_addr));

	/* page fault on task end : kill current task */
	if (fault_addr == TASK_RETURN_ADDRESS) {
		sys_exit(0);
		return;
	}

	/* get errors informations */
	int present = regs->err_code & 0x1 ? 1 : 0;
	int write = regs->err_code & 0x2 ? 1 : 0;
	int user = regs->err_code & 0x4 ? 1 : 0;
	int reserved = regs->err_code & 0x8 ? 1 : 0;
	int id = regs->err_code & 0x10 ? 1 : 0;

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
	vma->vm_start = fault_addr & PAGE_MASK;
	return;
good_area:
	/* write violation */
	if (write && !(vma->vm_flags & VM_WRITE))
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
	ret = do_no_page(current_task, vma, fault_addr);
	if (ret)
		goto bad_area;

	return;
bad_area:
	/* output message */
	printf("Page fault at address=%x | present=%d write-access=%d user-mode=%d reserved=%d instruction-fetch=%d (process %d at %x)\n",
	       fault_addr, present, write, user, reserved, id, current_task->pid, regs->eip);

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
	__asm__ volatile("mov %0, %%cr3" :: "r" (pgd));
	__asm__ volatile("mov %%cr0, %0" : "=r" (cr0));

	/* enable paging */
	cr0 |= 0x80000000;
	__asm__ volatile("mov %0, %%cr0" :: "r" (cr0));
}

/*
 * Clone a page table.
 */
static pmd_t clone_pmd(pmd_t *pmd_src)
{
	pte_t *ptes_src, *ptes_new;
	struct page *page;
	int ret, i;

	/* create a new page table */
	ptes_new = (pmd_t *) kmalloc_align(PAGE_SIZE);
	if (!ptes_new)
		return 0;

	/* reset page table */
	memset(ptes_new, 0, PAGE_SIZE);

	/* get table entries */
	ptes_src = (pte_t *) pmd_page(*pmd_src);

	/* copy physical pages */
	for (i = 0; i < PTRS_PER_PTE; i++) {
		if (!PTE_PAGE(ptes_src[i]))
			continue;

		/* try to get a page */
		page = __get_free_page(GFP_USER);
		if (!page)
			goto err;

		/* set page table entry */
		ret = set_pte(&ptes_new[i], PTE_PROT(ptes_src[i]), page);
		if (ret) {
			__free_page(page);
			goto err;
		}

		/* copy physical page */
		copy_page_physical(PTE_PAGE(ptes_src[i]) * PAGE_SIZE, PTE_PAGE(ptes_new[i]) * PAGE_SIZE);
	}

	return (pmd_t ) ptes_new | PAGE_TABLE;
err:
	kfree(ptes_new);
	return 0;
}

/*
 * Clone a page directory.
 */
pgd_t *clone_pgd(pgd_t *pgd_src)
{
	pmd_t *pmd_src, *pmd_dst, *pmd_kernel;
	pgd_t *pgd_new;
	int i;

	/* create a new page directory */
	pgd_new = (pgd_t *) kmalloc_align(PAGE_SIZE);
	if (!pgd_new)
		return NULL;

	/* reset page directory */
	memset(pgd_new, 0, PAGE_SIZE);

	/* get tables */
	pmd_src = pmd_offset(pgd_src);
	pmd_dst = pmd_offset(pgd_new);
	pmd_kernel = pmd_offset(pgd_kernel);

	/* copy page tables */
	for (i = 0; i < PTRS_PER_PTE; i++, pmd_src++, pmd_dst++, pmd_kernel++) {
		if (!pmd_page(*pmd_src))
			continue;

		/* if kernel page tables, just link */
		if (pmd_page(*pmd_src) == pmd_page(*pmd_kernel)) {
			*pmd_dst = *pmd_src;
		} else {
			*pmd_dst = clone_pmd(pmd_src);
			if (!pmd_dst) {
				free_pgd(pgd_new);
				return NULL;
			}
		}
	}

	return pgd_new;
}

/*
 * Free a page table.
 */
static void free_pmd(pmd_t *pmd)
{
	uint32_t page_idx;
	pte_t *ptes;
	int i;

	/* get table entries */
	ptes = (pte_t *) pmd_page(*pmd);

	/* free pages */
	for (i = 0; i < PTRS_PER_PTE; i++) {
		page_idx = PTE_PAGE(ptes[i]);
		if (page_idx > 0 && page_idx < nr_pages)
			__free_page(&page_array[page_idx]);
	}

	/* free page table */
	kfree(ptes);
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
	kfree(pgd);
}

/**
 * @brief Map a kernel page.
 */
static int map_kernel_page(uint32_t page_nr, uint32_t addr, int pgprot)
{
	pte_t *pte;

	/* make page table entry */
	pte = get_pte(addr, 1, pgd_kernel);
	if (!pte)
		return -ENOMEM;

	/* set page table entry */
	return set_pte(pte, pgprot, &page_array[page_nr]);
}

/*
 * Init paging.
 */
int init_paging(uint32_t end)
{
	uint32_t addr, last_kernel_addr, i;
	int ret;

	/* compute number of pages */
	nr_pages = end / PAGE_SIZE;

	/* allocate kernel page directory after kernel heap */
	last_kernel_addr = KHEAP_START + KHEAP_SIZE;
	pgd_kernel = (pgd_t *) last_kernel_addr;
	memset(pgd_kernel, 0, PAGE_SIZE);
	last_kernel_addr += PAGE_SIZE;

	/* allocate global pages array */
	page_array = (struct page *) last_kernel_addr;
	memset(page_array, 0, sizeof(struct page) * nr_pages);
	last_kernel_addr += sizeof(struct page) * nr_pages;

	/* init pages */
	for (i = 0; i < nr_pages; i++)
		page_array[i].page = i;

	/* map kernel code pages */
	for (i = 0, addr = 0; addr < last_kernel_addr; i++, addr += PAGE_SIZE) {
		/* mark page used */
		page_array[i].count = 1;
		page_array[i].kernel = 1;

		/* map to low memory */
		ret = map_kernel_page(i, addr, PAGE_READONLY);
		if (ret)
			return ret;

		/* map to high memory */
		ret = map_kernel_page(i, P2V(addr), PAGE_READONLY);
		if (ret)
			return ret;
	}

	/* map kernel pages to high memory */
	for (; i < nr_pages && P2V(addr) < KPAGE_END; i++, addr += PAGE_SIZE) {
		/* mark kernel page */
		page_array[i].kernel = 1;

		ret = map_kernel_page(i, P2V(addr), PAGE_KERNEL);
		if (ret)
			return ret;
	}

	/* register page fault handler */
	register_interrupt_handler(14, page_fault_handler);

	/* enable paging */
	switch_pgd(pgd_kernel);

	/* init page allocation */
	init_page_alloc();

	/* init page cache */
	return init_page_cache();
}
