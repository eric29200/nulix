#include <mm/mm.h>
#include <proc/sched.h>
#include <mm/paging.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>

/* global pages */
uint32_t nr_pages;
struct page *page_table;

/* page directories */
struct page_directory *kernel_pgd = NULL;

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
static uint32_t *get_pte(uint32_t address, uint8_t make, struct page_directory *pgd)
{
	uint32_t page_nr, table_idx;

	/* get page table */
	page_nr = address / PAGE_SIZE;
	table_idx = page_nr / 1024;

	/* table already assigned */
	if (pgd->tables[table_idx])
		return &pgd->tables[table_idx]->pages[page_nr % 1024];

	/* create a new page table */
	if (make) {
		pgd->tables[table_idx] = (struct page_table *) kmalloc_align(sizeof(struct page_table));
		if (!pgd->tables[table_idx])
			return NULL;

		/* set page table entry */
		memset(pgd->tables[table_idx], 0, PAGE_SIZE);
		pgd->tables_physical[table_idx] = (uint32_t) pgd->tables[table_idx] | 0x7;

		/* flush tlb */
		flush_tlb(address);

		return &pgd->tables[table_idx]->pages[page_nr % 1024];
	}

	return 0;
}

/*
 * Set a page table entry.
 */
static int set_pte(uint32_t *pte, int pgprot, struct page *page)
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
int remap_page_range(uint32_t start, uint32_t phys_addr, size_t size, struct page_directory *pgd, int pgprot)
{
	uint32_t address, page_idx, *pte;

	for (address = start; address < start + size; address += PAGE_SIZE, phys_addr += PAGE_SIZE) {
		/* get page table entry */
		pte = get_pte(address, 1, pgd);
		if (!pte)
			return -ENOMEM;

		/* set page table entry */
		page_idx = phys_addr / PAGE_SIZE;
		*pte = MK_PTE(page_idx, pgprot);
	}

	return 0;
}

/*
 * Unmap a page.
 */
static void unmap_page(uint32_t address, struct page_directory *pgd)
{
	uint32_t page_nr, table_idx, page_idx, *pte;

	/* get page table */
	page_nr = address / PAGE_SIZE;
	table_idx = page_nr / 1024;
	if (!pgd->tables[table_idx])
		return;

	/* get page */
	pte = &pgd->tables[table_idx]->pages[page_nr % 1024];
	if (!pte)
		return;

	/* free page */
	page_idx = PTE_PAGE(*pte);
	if (page_idx && page_idx < nr_pages)
		__free_page(&page_table[page_idx]);

	/* reset page table entry */
	*pte = 0;

	/* flush tlb */
	flush_tlb(address);
}

/*
 * Unmap pages.
 */
void unmap_pages(uint32_t start_address, uint32_t end_address, struct page_directory *pgd)
{
	uint32_t address;

	/* unmap all pages */
	for (address = start_address; address < end_address; address += PAGE_SIZE)
		unmap_page(address, pgd);
}

/*
 * Anonymous page mapping.
 */
static int do_anonymous_page(struct vm_area *vma, uint32_t *pte)
{
	struct page *page;
	int ret;

	/* try to get a page */
	page = __get_free_page();
	if (!page)
		return -ENOMEM;

	/* memzero page */
	memset((void *) PAGE_ADDRESS(page), 0, PAGE_SIZE);

	/* set page table entry */
	ret = set_pte(pte, vma->vm_page_prot, page);
	if (ret)
		goto err;

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
	uint32_t *pte;

	/* get page table entry */
	pte = get_pte(address, 1, task->mm->pgd);
	if (!pte)
		return -ENOMEM;

	/* page table entry already set */
	if (PTE_PAGE(*pte) != 0)
		return -EPERM;

	/* anonymous page mapping */
	if (!vma->vm_ops || !vma->vm_ops->nopage)
		return do_anonymous_page(vma, pte);

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
	uint32_t *pte, page_idx;
	struct page *page;

	/* get page table entry */
	pte = get_pte(address, 0, task->mm->pgd);
	if (!pte)
		return -EINVAL;

	/* get page */
	page_idx = PTE_PAGE(*pte);
	if (page_idx <= 0 || page_idx >= nr_pages)
		return -EINVAL;
	page = &page_table[page_idx];

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
void switch_page_directory(struct page_directory *pgd)
{
	uint32_t cr0;

	/* switch */
	__asm__ volatile("mov %0, %%cr3" :: "r" (pgd->tables_physical));
	__asm__ volatile("mov %%cr0, %0" : "=r" (cr0));

	/* enable paging */
	cr0 |= 0x80000000;
	__asm__ volatile("mov %0, %%cr0" :: "r" (cr0));
}


/*
 * Clone a page table.
 */
static struct page_table *clone_page_table(struct page_table *src)
{
	struct page_table *pgt;
	struct page *page;
	int ret, i;

	/* create a new page table */
	pgt = (struct page_table *) kmalloc_align(sizeof(struct page_table));
	if (!pgt)
		return NULL;

	/* reset page table */
	memset(pgt, 0, sizeof(struct page_table));

	/* copy physical pages */
	for (i = 0; i < 1024; i++) {
		if (!PTE_PAGE(src->pages[i]))
			continue;

		/* try to get a page */
		page = __get_free_page();
		if (!page) {
			kfree(pgt);
			return NULL;
		}

		/* set page table entry */
		ret = set_pte(&pgt->pages[i], PTE_PROT(src->pages[i]), page);
		if (ret) {
			__free_page(page);
			kfree(pgt);
			return NULL;
		}

		/* copy physical page */
		copy_page_physical(PTE_PAGE(src->pages[i]) * PAGE_SIZE, PTE_PAGE(pgt->pages[i]) * PAGE_SIZE);
	}

	return pgt;
}

/*
 * Clone a page directory.
 */
struct page_directory *clone_page_directory(struct page_directory *src)
{
	struct page_directory *ret;
	int i;

	/* create a new page directory */
	ret = (struct page_directory *) kmalloc_align(sizeof(struct page_directory));
	if (!ret)
		return NULL;

	/* reset page directory */
	memset(ret, 0, sizeof(struct page_directory));

	/* copy page tables */
	for (i = 0; i < 1024; i++) {
		if (!src->tables[i])
			continue;

		/* if kernel page tables, just link */
		if (kernel_pgd->tables[i] == src->tables[i]) {
			ret->tables[i] = src->tables[i];
			ret->tables_physical[i] = src->tables_physical[i];
		} else {
			ret->tables[i] = clone_page_table(src->tables[i]);
			if (!ret->tables[i]) {
				free_page_directory(ret);
				return NULL;
			}

			ret->tables_physical[i] = (uint32_t) ret->tables[i] | 0x07;
		}
	}

	return ret;
}

/*
 * Free a page table.
 */
static void free_page_table(struct page_table *pgt)
{
	uint32_t page_idx;
	int i;

	/* free pages */
	for (i = 0; i < 1024; i++) {
		page_idx = PTE_PAGE(pgt->pages[i]);
		if (page_idx > 0 && page_idx < nr_pages)
			__free_page(&page_table[page_idx]);
	}

	/* free page table */
	kfree(pgt);
}

/*
 * Free a page directory.
 */
void free_page_directory(struct page_directory *pgd)
{
	int i;

	if (!pgd)
		return;

	/* free page tables */
	for (i = 0; i < 1024; i++)
		if (pgd->tables[i] && pgd->tables[i] != kernel_pgd->tables[i])
			free_page_table(pgd->tables[i]);

	/* free page directory */
	kfree(pgd);
}

/**
 * @brief Map a kernel page.
 */
static int map_kernel_page(uint32_t page_nr, uint32_t addr, int pgprot)
{
	uint32_t *pte;

	/* make page table entry */
	pte = get_pte(addr, 1, kernel_pgd);
	if (!pte)
		return -ENOMEM;

	/* set page table entry */
	return set_pte(pte, pgprot, &page_table[page_nr]);
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

	/* limit memory to 1G */
	if (end > KPAGE_END - KPAGE_START)
		nr_pages = (KPAGE_END - KPAGE_START) / PAGE_SIZE;

	/* allocate kernel page directory after kernel heap */
	last_kernel_addr = KHEAP_START + KHEAP_SIZE;
	kernel_pgd = (struct page_directory *) last_kernel_addr;
	memset(kernel_pgd, 0, sizeof(struct page_directory));
	last_kernel_addr += sizeof(struct page_directory);

	/* allocate global page table */
	page_table = (struct page *) last_kernel_addr;
	memset(page_table, 0, sizeof(struct page) * nr_pages);
	last_kernel_addr += sizeof(struct page) * nr_pages;

	/* map kernel code pages */
	for (i = 0, addr = 0; addr < last_kernel_addr; i++, addr += PAGE_SIZE) {
		/* add page to used list */
		page_table[i].page = i;
		page_table[i].count = 1;

		/* map to low memory */
		ret = map_kernel_page(i, addr, PAGE_READONLY);
		if (ret)
			return ret;

		/* map to high memory */
		ret = map_kernel_page(i, P2V(addr), PAGE_READONLY);
		if (ret)
			return ret;
	}

	/* map kernel pages */
	for (; i < nr_pages && P2V(addr) < KPAGE_END; i++, addr += PAGE_SIZE) {
		/* add page to free list */
		page_table[i].page = i;

		/* map to high memory */
		ret = map_kernel_page(i, P2V(addr), PAGE_KERNEL);
		if (ret)
			return ret;
	}

	/* register page fault handler */
	register_interrupt_handler(14, page_fault_handler);

	/* enable paging */
	switch_page_directory(kernel_pgd);

	/* init page allocation */
	init_page_alloc();

	/* init page cache */
	return init_page_cache();
}
