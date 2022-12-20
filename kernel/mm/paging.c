#include <mm/mm.h>
#include <proc/sched.h>
#include <mm/paging.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>

/* global pages */
uint32_t nb_pages;
struct page_t *page_table;
struct list_head_t free_pages;

/* page directories */
struct page_directory_t *kernel_pgd = 0;

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
 * Get a free page.
 */
static struct page_t *__get_free_page()
{
	struct page_t *page;

	/* try to get a page */
	if (list_empty(&free_pages)) {
		/* no more pages : reclaim buffers */
		reclaim_buffers();

		if (list_empty(&free_pages))
			return NULL;
	}
	
	/* get first free page */
	page = list_first_entry(&free_pages, struct page_t, list);
	list_del(&page->list);

	return page;
}

/*
 * Free a page.
 */
static void __free_page(struct page_t *page)
{
	if (!page)
		return;

	list_add(&page->list, &free_pages);
}

/*
 * Get or create a page table entry from pgd at virtual address.
 */
static uint32_t *get_pte(uint32_t address, uint8_t make, struct page_directory_t *pgd)
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
		pgd->tables[table_idx] = (struct page_table_t *) kmalloc_align(sizeof(struct page_table_t));
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
static int set_pte(uint32_t *pte, int pgprot, struct page_t *page)
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
 * Map a page.
 */
int map_page(uint32_t address, struct page_directory_t *pgd, int pgprot)
{
	struct page_t *page;
	uint32_t *pte;
	int ret;

	/* get page table entry */
	pte = get_pte(address, 1, pgd);
	if (!pte)
		return -ENOMEM;

	/* page table entry already set */
	if (PTE_PAGE(*pte) != 0)
		return -EPERM;

	/* try to get a page */
	page = __get_free_page();
	if (!page)
		return -ENOMEM;

	/* set page table entry */
	ret = set_pte(pte, pgprot, page);
	if (ret) {
		__free_page(page);
		return ret;
	}

	/* flush TLB */
	flush_tlb(address);

	return 0;
}

/*
 * Map a page to a physical address.
 */
int map_page_phys(uint32_t address, uint32_t phys, struct page_directory_t *pgd, int pgprot)
{
	uint32_t page_idx, *pte;

	/* get page table entry */
	pte = get_pte(address, 1, pgd);
	if (!pte)
		return -ENOMEM;

	/* set page table entry */
	page_idx = phys / PAGE_SIZE;
	*pte = MK_PTE(page_idx, pgprot);

	return 0;
}

/*
 * Unmap a page.
 */
static void unmap_page(uint32_t address, struct page_directory_t *pgd)
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
	if (page_idx && page_idx < nb_pages)
		__free_page(&page_table[page_idx]);

	/* reset page table entry */
	*pte = 0;

	/* flush tlb */
	flush_tlb(address);
}

/*
 * Unmap pages.
 */
void unmap_pages(uint32_t start_address, uint32_t end_address, struct page_directory_t *pgd)
{
	uint32_t address;

	/* unmap all pages */
	for (address = start_address; address < end_address; address += PAGE_SIZE)
		unmap_page(address, pgd);
}

/*
 * Handle a no page fault.
 */
static int do_no_page(struct task_t *task, struct vm_area_t *vma, uint32_t address)
{
	int ret;

	/* handle no page */
	if (vma && vma->vm_ops && vma->vm_ops->nopage)
		return vma->vm_ops->nopage(vma, address);

	/* else just map page */
	ret = map_page(address, task->mm->pgd, PAGE_SHARED);
	if (ret)
		return ret;

	/* memzero page */
	memset((void *) PAGE_ALIGN_DOWN(address), 0, PAGE_SIZE);

	return 0;
}

/*
 * Page fault handler.
 */
void page_fault_handler(struct registers_t *regs)
{
	struct vm_area_t *vma;
	uint32_t fault_addr;

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
	/* user page fault : handle it */
	if (do_no_page(current_task, vma, fault_addr) == 0)
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
void switch_page_directory(struct page_directory_t *pgd)
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
static struct page_table_t *clone_page_table(struct page_table_t *src)
{
	struct page_table_t *pgt;
	struct page_t *page;
	int ret, i;

	/* create a new page table */
	pgt = (struct page_table_t *) kmalloc_align(sizeof(struct page_table_t));
	if (!pgt)
		return NULL;

	/* reset page table */
	memset(pgt, 0, sizeof(struct page_table_t));

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
		ret = set_pte(&pgt->pages[i], PAGE_READONLY, page);
		if (ret) {
			__free_page(page);
			kfree(pgt);
			return NULL;
		}

		/* set page */
		pgt->pages[i] |= PTE_PROT(src->pages[i]);
		copy_page_physical(PTE_PAGE(src->pages[i]) * PAGE_SIZE, PTE_PAGE(pgt->pages[i]) * PAGE_SIZE);
	}

	return pgt;
}

/*
 * Clone a page directory.
 */
struct page_directory_t *clone_page_directory(struct page_directory_t *src)
{
	struct page_directory_t *ret;
	int i;

	/* create a new page directory */
	ret = (struct page_directory_t *) kmalloc_align(sizeof(struct page_directory_t));
	if (!ret)
		return NULL;

	/* reset page directory */
	memset(ret, 0, sizeof(struct page_directory_t));

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
static void free_page_table(struct page_table_t *pgt)
{
	struct page_t *page;
	uint32_t page_idx;
	int i;

	for (i = 0; i < 1024; i++) {
		page_idx = PTE_PAGE(pgt->pages[i]);
		if (page_idx > 0 && page_idx < nb_pages) {
			page = &page_table[page_idx];
			list_add(&page->list, &free_pages);
		}
	}

	kfree(pgt);
}

/*
 * Free a page directory.
 */
void free_page_directory(struct page_directory_t *pgd)
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

/*
 * Get a free page.
 */
void *get_free_page()
{
	struct page_t *page;

	/* get a free page */
	page = __get_free_page();
	if (!page)
		return NULL;

	/* make virtual address */
	return (void *) (KPAGE_START + page->page * PAGE_SIZE);
}

/*
 * Free a page.
 */
void free_page(void *address)
{
	uint32_t page_idx;

	/* get page index */
	page_idx = ((uint32_t) address - KPAGE_START) / PAGE_SIZE;
	
	/* free page */
	if (page_idx && page_idx < nb_pages)
		__free_page(&page_table[page_idx]);
}

/*
 * Init paging.
 */
int init_paging(uint32_t start, uint32_t end)
{
	uint32_t address, last_kernel_addr, i, *pte;
	int ret;

	/* unused start address */
	UNUSED(start);

	/* compute number of pages */
	nb_pages = end / PAGE_SIZE;

	/* allocate kernel page directory */
	kernel_pgd = (struct page_directory_t *) kmalloc_align(sizeof(struct page_directory_t));
	memset(kernel_pgd, 0, sizeof(struct page_directory_t));

	/* allocate global page table after kernel heap */
	last_kernel_addr = KHEAP_START + KHEAP_SIZE;
	page_table = (struct page_t *) last_kernel_addr;
	last_kernel_addr += sizeof(struct page_t) * nb_pages;

	/* init pages */
	INIT_LIST_HEAD(&free_pages);
	for (i = 0; i < nb_pages; i++) {
		page_table[i].page = i;
	
		/* add pages to free list */
		if (i * PAGE_SIZE > last_kernel_addr)
			list_add_tail(&page_table[i].list, &free_pages);
	}	

	/* identity map kernel pages */
	for (i = 0, address = 0; address < last_kernel_addr; i++, address += PAGE_SIZE) {
		/* make page table entry */
		pte = get_pte(address, 1, kernel_pgd);
		if (!pte)
			return -ENOMEM;

		/* set page table entry */
		ret = set_pte(pte, PAGE_READONLY, &page_table[i]);
		if (ret)
			return ret;
	}

	/* map physical pages to highmem */
	for (i = 0, address = KPAGE_START; i < nb_pages; i++, address += PAGE_SIZE) {
		/* make page table entry */
		pte = get_pte(address, 1, kernel_pgd);
		if (!pte)
			return -ENOMEM;

		/* set page table entry */
		ret = set_pte(pte, PAGE_KERNEL, &page_table[i]);
		if (ret)
			return ret;
	}
	  
	/* register page fault handler */
	register_interrupt_handler(14, page_fault_handler);

	/* enable paging */
	switch_page_directory(kernel_pgd);

	return 0;
}
