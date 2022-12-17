#include <mm/mm.h>
#include <proc/sched.h>
#include <mm/paging.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>

/* page frames */
uint32_t *frames;
uint32_t nb_frames;

/* page directories */
struct page_directory_t *kernel_pgd = 0;

/* kernel pages */
int nb_kernel_pages;
static struct kernel_page_t *kernel_pages;
static struct list_head_t kernel_free_pages;

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
 * Get next free frame.
 */
static int32_t get_first_free_frame()
{
	uint32_t i, j;

	for (i = 0; i < nb_frames / 32; i++)
		if (frames[i] != 0xFFFFFFFF)
			for (j = 0; j < 32; j++)
				if (!(frames[i] & (0x1 << j)))
					return 32 * i + j;

	return -1;
}

/*
 * Set a frame.
 */
static void set_frame(uint32_t frame_addr)
{
	uint32_t frame = frame_addr / PAGE_SIZE;
	if (frame < nb_frames)
		frames[frame / 32] |= (0x1 << (frame % 32));
}

/*
 * Clear a frame.
 */
static void clear_frame(uint32_t frame_addr)
{
	uint32_t frame = frame_addr / PAGE_SIZE;
	if (frame < nb_frames)
		frames[frame / 32] &= ~(0x1 << (frame % 32));
}

/*
 * Allocate a frame.
 */
static int alloc_frame(uint32_t *pte, uint8_t kernel, uint8_t write)
{
	int32_t frame_idx;

	/* frame already allocated */
	if (PTE_PAGE(*pte) != 0)
		return -EPERM;

	/* get a new frame */
	frame_idx = get_first_free_frame();
	if (frame_idx < 0) {
		/* no more frames : reclaim buffers */
		reclaim_buffers();

		/* and retyrn */
		frame_idx = get_first_free_frame();
		if (frame_idx < 0)
			return -ENOMEM;
	}

	set_frame(PAGE_SIZE * frame_idx);
	*pte = MK_PTE(frame_idx, PAGE_PRESENT | (kernel ? 0 : PAGE_USER) | (write ? PAGE_RW : 0));

	return 0;
}

/*
 * Map a page.
 */
int map_page(uint32_t address, struct page_directory_t *pgd, uint8_t kernel, uint8_t write)
{
	uint32_t *pte;
	int ret;

	/* get page table entry */
	pte = get_pte(address, 1, pgd);
	if (!pte)
		return -ENOMEM;

	/* alloc frame */
	ret = alloc_frame(pte, kernel, write);
	if (ret)
		return ret;

	/* flush TLB */
	flush_tlb(address);

	return 0;
}

/*
 * Map pages.
 */
int map_pages(uint32_t start_address, uint32_t end_address, struct page_directory_t *pgd, uint8_t kernel, uint8_t write)
{
	uint32_t address;
	int ret;

	for (address = start_address; address < end_address; address += PAGE_SIZE) {
		ret = map_page(address, pgd, kernel, write);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Map a page to a physical address.
 */
int map_page_phys(uint32_t address, uint32_t phys, struct page_directory_t *pgd, uint8_t kernel, uint8_t write)
{
	uint32_t frame_idx, *pte;

	/* get page table entry */
	pte = get_pte(address, 1, pgd);
	if (!pte)
		return -ENOMEM;

	/* compute frame index */
	frame_idx = phys / PAGE_SIZE;

	/* set page table entry */
	set_frame(frame_idx * PAGE_SIZE);
	*pte = MK_PTE(frame_idx, PAGE_PRESENT | (kernel ? 0 : PAGE_USER) | (write ? PAGE_RW : 0));

	return 0;
}

/*
 * Unmap a page.
 */
static void unmap_page(uint32_t address, struct page_directory_t *pgd)
{
	uint32_t page_nr, table_idx, *pte;

	/* get page table */
	page_nr = address / PAGE_SIZE;
	table_idx = page_nr / 1024;
	if (!pgd->tables[table_idx])
		return;

	/* get page */
	pte = &pgd->tables[table_idx]->pages[page_nr % 1024];
	if (!pte)
		return;

	/* clear frame */
	if (PTE_PAGE(*pte))
		clear_frame(PTE_PAGE(*pte) * PAGE_SIZE);

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
	ret = map_page(address, task->mm->pgd, 0, 1);
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
 * Get page table entry from pgd at virtual address. If make is set and the page doesn't exist, create it.
 */
uint32_t *get_pte(uint32_t address, uint8_t make, struct page_directory_t *pgd)
{
	uint32_t page_nr, table_idx, tmp;

	/* get page table */
	page_nr = address / PAGE_SIZE;
	table_idx = page_nr / 1024;

	/* table already assigned */
	if (pgd->tables[table_idx])
		return &pgd->tables[table_idx]->pages[page_nr % 1024];

	/* create a new page table */
	if (make) {
		pgd->tables[table_idx] = (struct page_table_t *) kmalloc_align_phys(sizeof(struct page_table_t), &tmp);
		if (!pgd->tables[table_idx])
			return NULL;

		/* set page table entry */
		memset(pgd->tables[table_idx], 0, PAGE_SIZE);
		pgd->tables_physical[table_idx] = tmp | 0x7;

		/* flush tlb */
		flush_tlb(address);

		return &pgd->tables[table_idx]->pages[page_nr % 1024];
	}

	return 0;
}

/*
 * Clone a page table.
 */
static struct page_table_t *clone_page_table(struct page_table_t *src, uint32_t *phys_addr)
{
	struct page_table_t *ret;
	int i;

	/* create a new page table */
	ret = (struct page_table_t *) kmalloc_align_phys(sizeof(struct page_table_t), phys_addr);
	if (!ret)
		return NULL;

	/* reset page table */
	memset(ret, 0, sizeof(struct page_table_t));

	/* copy physical pages */
	for (i = 0; i < 1024; i++) {
		if (!PTE_PAGE(src->pages[i]))
			continue;

		/* alloc a frame */
		if (alloc_frame(&ret->pages[i], 0, 0) == -1) {
			kfree(ret);
			return NULL;
		}

		/* set page */
		ret->pages[i] |= PTE_PROT(src->pages[i]);
		copy_page_physical(PTE_PAGE(src->pages[i]) * PAGE_SIZE, PTE_PAGE(ret->pages[i]) * PAGE_SIZE);
	}

	return ret;
}

/*
 * Clone a page directory.
 */
struct page_directory_t *clone_page_directory(struct page_directory_t *src)
{
	struct page_directory_t *ret;
	uint32_t phys;
	int i;

	/* create a new page directory */
	ret = (struct page_directory_t *) kmalloc_align_phys(sizeof(struct page_directory_t), &phys);
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
			ret->tables[i] = clone_page_table(src->tables[i], &phys);
			if (!ret->tables[i]) {
				free_page_directory(ret);
				return NULL;
			}

			ret->tables_physical[i] = phys | 0x07;
		}
	}

	return ret;
}

/*
 * Free a page table.
 */
static void free_page_table(struct page_table_t *pgt)
{
	int i;

	for (i = 0; i < 1024; i++)
		if (PTE_PAGE(pgt->pages[i]))
			clear_frame(PTE_PAGE(pgt->pages[i]) * PAGE_SIZE);

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
	struct kernel_page_t *page;
	int ret;

	/* no more free pages */
	if (list_empty(&kernel_free_pages))
		return NULL;

	/* get first free page */
	page = list_first_entry(&kernel_free_pages, struct kernel_page_t, list);

	/* alloc frame */
	ret = alloc_frame(page->pte, 1, 1);
	if (ret)
		return NULL;

	/* flush TLB */
	flush_tlb(page->address);

	/* remove page from list */
	list_del(&page->list);

	return (void *) page->address;
}

/*
 * Free a page.
 */
void free_page(void *address)
{
	struct kernel_page_t *page;
	int page_nr;

	/* get kernel page */
	page_nr = ((uint32_t) address - KPAGE_START) / PAGE_SIZE;
	page = &kernel_pages[page_nr];

	/* unmap page */
	unmap_page((uint32_t) address, kernel_pgd);

	/* add page to free list */
	list_add(&page->list, &kernel_free_pages);
}

/*
 * Init paging.
 */
int init_paging(uint32_t start, uint32_t end)
{
	uint32_t address;
	int ret, i;

	/* unused start address */
	UNUSED(start);

	/* reset frames */
	nb_frames = end / PAGE_SIZE;
	frames = (uint32_t *) kmalloc(nb_frames / 32);
	memset(frames, 0, nb_frames / 32);

	/* allocate kernel page directory */
	kernel_pgd = (struct page_directory_t *) kmalloc_align_phys(sizeof(struct page_directory_t), NULL);
	memset(kernel_pgd, 0, sizeof(struct page_directory_t));

	/* map kernel pages (kernel code + kernel heap) */
	ret = map_pages(0, KMEM_SIZE, kernel_pgd, 0, 0);
	if (ret)
		return ret;

	/* compute number of kernel pages (limit to 1/4 of total memory) */
	if (KPAGE_END - KPAGE_START > end / 4)
		nb_kernel_pages = end / (PAGE_SIZE * 4);
	else
		nb_kernel_pages = (KPAGE_END - KPAGE_START) / PAGE_SIZE;

	/* create kernel pages tables */
	for (i = 0, address = KPAGE_START; i < nb_kernel_pages; i++, address += PAGE_SIZE)
		get_pte(address, 1, kernel_pgd);

	/* create kernel pages */
	INIT_LIST_HEAD(&kernel_free_pages);
	kernel_pages = (struct kernel_page_t *) kmalloc_align_phys(sizeof(struct kernel_page_t) * nb_kernel_pages, NULL);
	for (i = 0, address = KPAGE_START; i < nb_kernel_pages; i++, address += PAGE_SIZE) {
		kernel_pages[i].pte = get_pte(address, 0, kernel_pgd);
		kernel_pages[i].address = address;
		list_add_tail(&kernel_pages[i].list, &kernel_free_pages);
	}

	/* register page fault handler */
	register_interrupt_handler(14, page_fault_handler);

	/* enable paging */
	switch_page_directory(kernel_pgd);

	return 0;
}
