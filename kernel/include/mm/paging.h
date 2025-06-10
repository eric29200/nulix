#ifndef _MM_PAGING_H_
#define _MM_PAGING_H_

#include <x86/interrupt.h>
#include <mm/mm.h>
#include <lib/list.h>
#include <stddef.h>

/* page directory, page middle directory (= page table) and page table entry */
typedef uint32_t pgd_t;
typedef uint32_t pmd_t;
typedef uint32_t pte_t;

#define PAGE_SHIFT			12
#define PAGE_SIZE			(1 << PAGE_SHIFT)
#define PAGE_MASK			(~(PAGE_SIZE - 1))
#define PAGE_ALIGNED(addr)		(((addr) & PAGE_MASK) == (addr))
#define PAGE_ALIGN_DOWN(addr)		((addr) & PAGE_MASK)
#define PAGE_ALIGN_UP(addr)		(((addr) + PAGE_SIZE - 1) & PAGE_MASK)
#define ALIGN_UP(addr, size)		(((addr) + size - 1) & (~(size - 1)))
#define PTRS_PER_PTE			1024
#define PTRS_PER_PMD			1
#define PGDIR_SHIFT			22
#define PGDIR_SIZE			(1UL << PGDIR_SHIFT)
#define PGDIR_MASK			(~(PGDIR_SIZE - 1))
#define PMD_SHIFT			22
#define PMD_SIZE			(1UL << PMD_SHIFT)
#define PMD_MASK			(~(PMD_SIZE - 1))
#define PAGE_OFFSET			0xC0000000
#define PTE_TABLE_MASK			((PTRS_PER_PTE - 1) * sizeof(pte_t))
#define PMD_TABLE_MASK			((PTRS_PER_PMD - 1) * sizeof(pmd_t))

#define PAGE_PRESENT			0x001
#define PAGE_RW				0x002
#define PAGE_USER			0x004
#define PAGE_PCD			0x010
#define PAGE_ACCESSED			0x020
#define PAGE_DIRTY			0x040
#define PAGE_PROTNONE			0x080

#define PAGE_NONE			(PAGE_PRESENT | PAGE_ACCESSED)
#define PAGE_SHARED			(PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_ACCESSED)
#define PAGE_COPY			(PAGE_PRESENT | PAGE_USER | PAGE_ACCESSED)
#define PAGE_READONLY			(PAGE_PRESENT | PAGE_USER | PAGE_ACCESSED)
#define PAGE_KERNEL			(PAGE_PRESENT | PAGE_RW | PAGE_DIRTY | PAGE_ACCESSED)
#define PAGE_TABLE			(PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_ACCESSED | PAGE_DIRTY)

#define __P000				PAGE_NONE
#define __P001				PAGE_READONLY
#define __P010				PAGE_COPY
#define __P011				PAGE_COPY
#define __P100				PAGE_READONLY
#define __P101				PAGE_READONLY
#define __P110				PAGE_COPY
#define __P111				PAGE_COPY
#define __S000				PAGE_NONE
#define __S001				PAGE_READONLY
#define __S010				PAGE_SHARED
#define __S011				PAGE_SHARED
#define __S100				PAGE_READONLY
#define __S101				PAGE_READONLY
#define __S110				PAGE_SHARED
#define __S111				PAGE_SHARED

#define GFP_KERNEL			0
#define GFP_HIGHUSER			1
#define NR_ZONES			2

#define __pa(addr)			((uint32_t)(addr) - PAGE_OFFSET)
#define __va(addr)			((void *)((uint32_t)(addr) + PAGE_OFFSET))
#define page_address(page)		((page)->virtual ? (page)->virtual : __va(((page) - page_array) * PAGE_SIZE))
#define MAP_NR(addr)			(__pa(addr) >> PAGE_SHIFT)
#define VALID_PAGE(page)		((uint32_t) (page - page_array) < nr_pages)

#define __mk_pte(page_nr, prot)		(((page_nr) << PAGE_SHIFT) | (prot))
#define mk_pte(page, prot)		__mk_pte((page) - page_array, (prot))
#define mk_pte_phys(phys, prot)		__mk_pte((phys) >> PAGE_SHIFT, prot)
#define pmd_none(pmd)			(!(pmd))
#define pte_page(pte)			(page_array + ((uint32_t)(((pte) >> PAGE_SHIFT))))
#define pte_prot(pte)			((pte) & (PAGE_SIZE - 1))
#define pte_clear(pte)			(*(pte) = 0)
#define pte_none(pte)			(!(pte))
#define pte_present(pte)		((pte) & (PAGE_PRESENT | PAGE_PROTNONE))
#define pte_offset(pmd, addr) 		((pte_t *) (pmd_page(*pmd) + ((addr >> 10) & ((PTRS_PER_PTE - 1) << 2))))

static inline pte_t pte_mkwrite(pte_t pte)
{
	pte |= PAGE_RW; return pte;
}

static inline pte_t pte_wrprotect(pte_t pte)
{
	pte &= ~PAGE_RW; return pte;
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	pte |= PAGE_DIRTY; return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte &= ~PAGE_DIRTY; return pte;
}

static inline pte_t pte_mkold(pte_t pte)
{
	pte &= ~PAGE_ACCESSED; return pte;
}

/* page allocation */
#define __get_free_page(priority)	__get_free_pages(priority, 0)
#define __free_page(page)		__free_pages(page, 0)
#define get_free_page()			get_free_pages(0)
#define free_page(addr)			free_pages(addr, 0)


/* defined in paging.c */
extern uint32_t nr_pages;
extern struct page *page_array;
extern pgd_t *pgd_kernel;
extern uint32_t totalram_pages;
extern uint32_t page_cache_size;

/*
 * Page structure.
 */
struct page {
	int 			count;					/* reference count */
	uint8_t			priority;				/* priority : GFP_KERNEL or GFP_USER */
	struct inode *		inode;					/* inode */
	off_t			offset;					/* offset in inode */
	struct buffer_head *	buffers;				/* buffers of this page */
	void *			virtual;				/* virtual address (used to map high pages in kernel space) */
	void *			private;				/* used for page allocation */
	struct list_head	list;					/* next page */
	struct page *		next_hash;				/* next page in hash table */
	struct page *		prev_hash;				/* previous page in hash table */
};

/* paging */
int init_paging(uint32_t kernel_start, uint32_t kernel_end, uint32_t mem_end);
size_t zap_page_range(pgd_t *pgd, uint32_t start_address, size_t size);
int copy_page_range(pgd_t *pgd_src, pgd_t *pgd_dst, struct vm_area *vma);
int remap_page_range(uint32_t start, uint32_t phys_addr, size_t size, int pgprot);
void switch_pgd(pgd_t *pgd);
void page_fault_handler(struct registers *regs);
pgd_t *create_page_directory();
void free_pgd(pgd_t *pgd);
void flush_tlb_page(pgd_t *pgd, uint32_t address);
void flush_tlb(pgd_t *pgd);

/* page allocation */
int init_page_alloc(uint32_t kernel_start, uint32_t kernel_end);
uint32_t nr_free_pages();
struct page *__get_free_pages(int priority, uint32_t order);
void __free_pages(struct page *page, uint32_t order);
void *get_free_pages(uint32_t order);
void free_pages(void *address, uint32_t order);
void reclaim_pages();

/* page cache */
void init_page_cache();
struct page *find_page(struct inode *inode, off_t offset);
void add_to_page_cache(struct page *page, struct inode *inode, off_t offset);
void remove_from_page_cache(struct page *page);
void truncate_inode_pages(struct inode *inode, off_t start);

/*
 * Get page directory offset for an address.
 */
static inline pgd_t *pgd_offset(pgd_t *pgd, uint32_t address)
{
	return pgd + (address >> PGDIR_SHIFT);
}

/*
 * Get page table offset for an address.
 */
static inline pmd_t *pmd_offset(pgd_t *pgd)
{
	return (pmd_t *) pgd;
}

/*
 * Get page table entry.
 */
static inline uint32_t pmd_page(pmd_t pmd)
{
	return (uint32_t) __va(pmd & PAGE_MASK);
}

#endif
