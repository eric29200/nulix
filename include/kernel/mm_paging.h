#ifndef _MM_PAGING_H_
#define _MM_PAGING_H_

#include <kernel/isr.h>
#include <stddef.h>

#define PAGE_SIZE             0x1000
#define PAGE_MASK             (~(PAGE_SIZE - 1))
#define PAGE_ALIGNED(addr)    (((addr) & PAGE_MASK) == 0)
#define PAGE_ALIGN(addr)      (((addr) + PAGE_SIZE - 1) & PAGE_MASK)
#define ALIGN_UP(addr, size)  (((addr) + size - 1) & (~(size - 1)))

/* defined in paging.c */
extern uint32_t placement_address;
extern uint32_t *frames;
extern uint32_t nb_frames;
extern struct page_directory_t *kernel_pgd;
extern struct page_directory_t *current_pgd;

/*
 * Page structure.
 */
struct page_t {
  uint32_t present :1;    /* page present in memory */
  uint32_t rw :1;         /* read only if clear */
  uint32_t user :1;       /* supervisor level if clear */
  uint32_t accessed :1;   /* set if the page has been accessed */
  uint32_t dirty :1;      /* set if the page has not been written */
  uint32_t unused :7;     /* reserved bits */
  uint32_t frame :20;     /* frame address */
};

/*
 * Page table structure.
 */
struct page_table_t {
  struct page_t pages[1024];
};

/*
 * Page directory structure.
 */
struct page_directory_t {
  struct page_table_t *tables[1024];  /* pointers to page tables */
  uint32_t tables_physical[1024];     /* pointers to page tables (physical addresses) */
  uint32_t physical_addr;             /* physical address of tables_physical */
};

void init_paging(uint32_t start, uint32_t end);
struct page_t *get_page(uint32_t address, uint8_t make, struct page_directory_t *pgd);
int32_t alloc_frame(struct page_t *page, uint8_t kernel, uint8_t write);
void free_frame(struct page_t *page);
void switch_page_directory(struct page_directory_t *pgd);
void page_fault_handler(struct registers_t regs);

#endif
