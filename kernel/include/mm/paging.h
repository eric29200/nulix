#ifndef _MM_PAGING_H_
#define _MM_PAGING_H_

#include <x86/interrupt.h>
#include <lib/list.h>
#include <stddef.h>

#define PAGE_SHIFT			12
#define PAGE_SIZE			(1 << PAGE_SHIFT)
#define PAGE_MASK			(~(PAGE_SIZE - 1))
#define PAGE_ALIGNED(addr)		(((addr) & PAGE_MASK) == (addr))
#define PAGE_ALIGN_DOWN(addr)		((addr) & PAGE_MASK)
#define PAGE_ALIGN_UP(addr)		(((addr) + PAGE_SIZE - 1) & PAGE_MASK)
#define ALIGN_UP(addr, size)		(((addr) + size - 1) & (~(size - 1)))
#define PTRS_PER_PTE			1024
#define PGDIR_SHIFT			22
#define PAGE_OFFSET			0xC0000000

#define PAGE_PRESENT			0x001
#define PAGE_RW				0x002
#define PAGE_USER			0x004
#define PAGE_PCD			0x010
#define PAGE_ACCESSED			0x020
#define PAGE_DIRTY			0x040

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

#define PTE_PAGE(pte)			((pte) >> PAGE_SHIFT)
#define PTE_PROT(pte)			((pte) & (PAGE_SIZE - 1))
#define MK_PTE(page, prot)		(((page) << PAGE_SHIFT) | (prot))

#define __pa(addr)			((uint32_t)(addr) - PAGE_OFFSET)
#define __va(addr)			((void *)((uint32_t)(addr) + PAGE_OFFSET))
#define MAP_NR(addr)			(__pa(addr) >> PAGE_SHIFT)
#define PAGE_ADDRESS(p)			(PAGE_OFFSET + (p)->page * PAGE_SIZE)

#define GFP_KERNEL			0
#define GFP_USER			1

/* page directory, page middle directory (= page table) and page table entry */
typedef uint32_t pgd_t;
typedef uint32_t pmd_t;
typedef uint32_t pte_t;

/* defined in paging.c */
extern uint32_t nr_pages;
extern struct page *page_array;
extern pgd_t *pgd_kernel;

/*
 * Page structure.
 */
struct page {
	uint32_t		page;					/* page number */
	int 			count;					/* reference count */
	uint8_t			kernel;					/* mapped in kernel ? */
	struct inode *		inode;					/* inode */
	off_t			offset;					/* offset in inode */
	struct buffer_head *	buffers;				/* buffers of this page */
	struct list_head	list;					/* next page */
	struct page *		next_hash;
	struct page *		prev_hash;
};

/* paging */
int init_paging(uint32_t start, uint32_t end);
void unmap_pages(uint32_t start_address, uint32_t end_address, pgd_t *pgd);
int remap_page_range(uint32_t start, uint32_t phys_addr, size_t size, pgd_t *pgd, int pgprot);
void switch_pgd(pgd_t *pgd);
void page_fault_handler(struct registers *regs);
pgd_t *clone_pgd(pgd_t *pgd);
void free_pgd(pgd_t *pgd);

/* page allocation */
void init_page_alloc(uint32_t last_kernel_addr);
struct page *__get_free_page(int priority);
void *get_free_page();
void __free_page(struct page *page);
void free_page(void *address);
void reclaim_pages();

/* page cache */
void init_page_cache();
struct page *find_page(struct inode *inode, off_t offset);
void add_to_page_cache(struct page *page, struct inode *inode, off_t offset);
void remove_from_page_cache(struct page *page);
void update_vm_cache(struct inode *inode, const char *buf, size_t pos, size_t count);
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
