#include <mm/mm.h>
#include <proc/sched.h>
#include <mm/paging.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>

#define PAGE_HASH_BITS			11
#define PAGE_HASH_SIZE			(1 << PAGE_HASH_BITS)

/* global pages */
uint32_t nr_pages;
struct page *page_table;
static struct list_head free_pages;
static struct list_head used_pages;
static struct htable_link **page_htable = NULL;

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
 * Get a free page.
 */
struct page *__get_free_page()
{
	struct page *page;

	/* try to get a page */
	if (list_empty(&free_pages)) {
		/* no more pages */
		reclaim_pages();

		if (list_empty(&free_pages))
			return NULL;
	}

	/* get first free page */
	page = list_first_entry(&free_pages, struct page, list);
	page->inode = NULL;
	page->offset = 0;
	page->buffers = NULL;
	page->count = 1;

	/* update lists */
	list_del(&page->list);
	list_add(&page->list, &used_pages);

	return page;
}

/*
 * Free a page.
 */
void __free_page(struct page *page)
{
	if (!page)
		return;

	page->count--;
	if (!page->count) {
		page->inode = NULL;
		list_del(&page->list);
		list_add(&page->list, &free_pages);
	}
}

/*
 * Get a free page.
 */
void *get_free_page()
{
	struct page *page;

	/* get a free page */
	page = __get_free_page();
	if (!page)
		return NULL;

	/* make virtual address */
	return (void *) P2V(page->page * PAGE_SIZE);
}

/*
 * Free a page.
 */
void free_page(void *address)
{
	uint32_t page_idx;

	/* get page index */
	page_idx = MAP_NR((uint32_t) address);

	/* free page */
	if (page_idx && page_idx < nr_pages)
		__free_page(&page_table[page_idx]);
}

/*
 * Reclaim pages, when memory is low.
 */
void reclaim_pages()
{
	struct page *page;
	uint32_t i;

	for (i = 0; i < nr_pages; i++) {
		page = &page_table[i];

		/* skip used pages */
		if (page->count > 1)
			continue;

		/* skip shared memory pages */
		if (page->inode && page->inode->i_shm == 1)
			continue;

		/* is it a buffer cached page ? */
		if (page->buffers) {
			try_to_free_buffer(page->buffers);
			continue;
		}

		/* is it a page cached page ? */
		if (page->inode) {
			/* remove from page cache */
			htable_delete(&page->htable);

			/* free page */
			__free_page(page);
		}
	}
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
 * Map a page.
 */
int map_page(uint32_t address, struct page_directory *pgd, int pgprot)
{
	struct page *page;
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
	struct page *page;
	uint32_t page_idx;
	int i;

	for (i = 0; i < 1024; i++) {
		page_idx = PTE_PAGE(pgt->pages[i]);
		if (page_idx > 0 && page_idx < nr_pages) {
			page = &page_table[page_idx];
			list_del(&page->list);
			list_add(&page->list, &free_pages);
		}
	}

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

/*
 * Hash an inode/offset.
 */
static inline uint32_t __page_hashfn(struct inode *inode, off_t offset)
{
#define i (((uint32_t) inode) / (sizeof(struct inode) & ~(sizeof(struct inode) - 1)))
#define o (offset >> PAGE_SHIFT)
#define s(x) ((x) + ((x) >> PAGE_HASH_BITS))
	return s(i + o) & (PAGE_HASH_SIZE);
#undef i
#undef o
#undef s
}

/*
 * Find a page in hash table.
 */
struct page *find_page(struct inode *inode, off_t offset)
{
	struct htable_link *node;
	struct page *page;

	/* try to find buffer in cache */
	node = htable_lookup(page_htable, __page_hashfn(inode, offset), PAGE_HASH_BITS);
	while (node) {
		page = htable_entry(node, struct page, htable);
		if (page->inode == inode && page->offset == offset) {
			page->count++;
			return page;
		}

		node = node->next;
	}

	return NULL;
}

/*
 * Cache a page.
 */
void add_to_page_cache(struct page *page, struct inode *inode, off_t offset)
{
	/* set page */
	page->count++;
	page->inode = inode;
	page->offset = offset;

	/* cache page */
	htable_delete(&page->htable);
	htable_insert(page_htable, &page->htable, __page_hashfn(inode, offset), PAGE_HASH_BITS);

	/* add page to inode */
	list_del(&page->list);
	list_add(&page->list, &inode->i_pages);
}

/*
 * Init page cache.
 */
int init_page_cache()
{
	uint32_t addr, nr, i;

	/* allocate page hash table */
	nr = 1 + nr_pages * sizeof(struct htable_link *) / PAGE_SIZE;
	for (i = 0; i < nr; i++) {
		/* get a free page */
		addr = (uint32_t) get_free_page();
		if (!addr)
			return -ENOMEM;

		/* reset page */
		memset((void *) addr, 0, PAGE_SIZE);

		/* set buffer hash table */
		if (i == 0)
			page_htable = (struct htable_link **) addr;
	}

	return 0;
}

/*
 * Init paging.
 */
int init_paging(uint32_t start, uint32_t end)
{
	uint32_t addr, last_kernel_addr, i, *pte;
	int ret;

	/* unused start address */
	UNUSED(start);

	/* compute number of pages */
	nr_pages = end / PAGE_SIZE;

	/* allocate kernel page directory */
	kernel_pgd = (struct page_directory *) kmalloc_align(sizeof(struct page_directory));
	memset(kernel_pgd, 0, sizeof(struct page_directory));

	/* allocate global page table after kernel heap */
	last_kernel_addr = KHEAP_START + KHEAP_SIZE;
	page_table = (struct page *) last_kernel_addr;
	memset(page_table, 0, sizeof(struct page) * nr_pages);
	last_kernel_addr += sizeof(struct page) * nr_pages;

	/* init pages */
	INIT_LIST_HEAD(&free_pages);
	INIT_LIST_HEAD(&used_pages);
	for (i = 0; i < nr_pages; i++) {
		page_table[i].page = i;

		/* add pages to used/free list */
		if (i * PAGE_SIZE < last_kernel_addr) {
			page_table[i].count = 1;
			list_add_tail(&page_table[i].list, &used_pages);
		} else {
			list_add_tail(&page_table[i].list, &free_pages);
		}
	}

	/* identity map kernel pages */
	for (i = 0, addr = 0; addr < last_kernel_addr; i++, addr += PAGE_SIZE) {
		/* make page table entry */
		pte = get_pte(addr, 1, kernel_pgd);
		if (!pte)
			return -ENOMEM;

		/* set page table entry */
		ret = set_pte(pte, PAGE_READONLY, &page_table[i]);
		if (ret)
			return ret;
	}

	/* map physical pages to highmem */
	for (i = 0, addr = KPAGE_START; i < nr_pages; i++, addr += PAGE_SIZE) {
		/* make page table entry */
		pte = get_pte(addr, 1, kernel_pgd);
		if (!pte)
			return -ENOMEM;

		/* set page table entry */
		ret = set_pte(pte, V2P(addr) < last_kernel_addr ? PAGE_READONLY : PAGE_KERNEL, &page_table[i]);
		if (ret)
			return ret;
	}

	/* register page fault handler */
	register_interrupt_handler(14, page_fault_handler);

	/* enable paging */
	switch_page_directory(kernel_pgd);

	/* init page cache */
	return init_page_cache();
}
