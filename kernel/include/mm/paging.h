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

#define PAGE_PRESENT			0x001
#define PAGE_RW				0x002
#define PAGE_USER			0x004
#define PAGE_PCD			0x010
#define PAGE_ACCESSED			0x020
#define PAGE_DIRTY			0x040

#define PTE_PAGE(pte)			((pte) >> PAGE_SHIFT)
#define PTE_PROT(pte)			((pte) & (PAGE_SIZE - 1))
#define MK_PTE(page, prot)		(((page) << PAGE_SHIFT) | (prot))

/* defined in paging.c */
extern uint32_t placement_address;
extern uint32_t *frames;
extern uint32_t nb_frames;
extern struct page_directory_t *kernel_pgd;

/*
 * Page directory structure.
 */
struct page_directory_t {
	struct page_table_t * 	tables[1024];			/* pointers to page tables */
	uint32_t		tables_physical[1024];		/* pointers to page tables (physical addresses) */
};

/*
 * Page table structure.
 */
struct page_table_t {
	uint32_t 		pages[1024];				/* pages */
};

/*
 * Kernel page structure.
 */
struct kernel_page_t {
	uint32_t *		pte;				/* page table entry */
	uint32_t		address;			/* virtual address */
	struct list_head_t	list;				/* next page */
};

int init_paging(uint32_t start, uint32_t end);
uint32_t *get_pte(uint32_t address, uint8_t make, struct page_directory_t *pgd);
int map_page(uint32_t address, struct page_directory_t *pgd, uint8_t kernel, uint8_t write);
int map_pages(uint32_t start_address, uint32_t end_address, struct page_directory_t *pgd, uint8_t kernel, uint8_t write);
int map_page_phys(uint32_t address, uint32_t phys, struct page_directory_t *pgd, uint8_t kernel, uint8_t write);
void unmap_pages(uint32_t start_address, uint32_t end_address, struct page_directory_t *pgd);
void switch_page_directory(struct page_directory_t *pgd);
void page_fault_handler(struct registers_t *regs);
struct page_directory_t *clone_page_directory(struct page_directory_t *pgd);
void free_page_directory(struct page_directory_t *pgd);

#endif
