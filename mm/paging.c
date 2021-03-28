#include <mm/paging.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* placement address */
uint32_t placement_address = 0;

/* bit frames */
uint32_t *frames;
uint32_t nb_frames;

/* page directories */
struct page_directory_t *kernel_pgd = 0;
struct page_directory_t *current_pgd = 0;

/*
 * Allocate physical memory.
 */
uint32_t kmalloc(uint32_t size, uint8_t align, uint32_t *phys)
{
  uint32_t ret;

  /* align adress on PAGE boundary */
  if (align == 1 && (placement_address & 0xFFFFF000)) {
    placement_address &= 0xFFFFF000;
    placement_address += PAGE_SIZE;
  }

  /* get physical memory */
  if (phys)
    *phys = placement_address;

  /* update placement_address */
  ret = placement_address;
  placement_address += size;
  return ret;
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
  frames[frame / 32] |= (0x1 << (frame % 32));
}

/*
 * Allocate a new frame.
 */
void alloc_frame(struct page_t *page, uint8_t kernel, uint8_t write)
{
  int32_t free_frame_idx;

  /* frame already allocated */
  if (page->frame != 0)
    return;

  /* get a new frame */
  free_frame_idx = get_first_free_frame();
  set_frame(PAGE_SIZE * free_frame_idx);
  page->present = 1;
  page->frame = free_frame_idx;
  page->rw = write ? 1 : 0;
  page->user = kernel ? 0 : 1;
}

/*
 * Clear a frame.
 */
static void clear_frame(uint32_t frame_addr)
{
  uint32_t frame = frame_addr / PAGE_SIZE;
  frames[frame / 32] &= ~(0x1 << (frame % 32));
}

/*
 * Free a frame.
 */
void free_frame(struct page_t *page)
{
  if (!(page->frame))
    return;

  clear_frame(page->frame);
  page->frame = 0;
}

/*
 * Page fault handler.
 */
void page_fault_handler(struct registers_t regs)
{
  uint32_t fault_addr;

  /* faulting address is stored in CR2 register */
  __asm__ volatile("mov %%cr2, %0" : "=r" (fault_addr));

  /* get erros informations */
  int present = !(regs.err_code & 0x1);
  int rw = regs.err_code & 0x2;
  int user = regs.err_code & 0x4;
  int reserved = regs.err_code & 0x8;
  int id = regs.err_code & 0x10;

  /* output message and panic */
  printf("\nPage fault at address=%x | present=%d read-only=%d user-mode=%d reserved=%d instruction-fetch=%d",
         fault_addr, present, rw, user, reserved, id);
  printf("PANIC\n");
  for (;;);
}

/*
 * Init paging.
 */
void init_paging(uint32_t start, uint32_t end)
{
  uint32_t i;

  /* set placement address (some memory is reserved for the heap structure) */
  placement_address = start + 0x100;

  /* set frames */
  nb_frames = end / PAGE_SIZE;
  frames = (uint32_t *) kmalloc(nb_frames / 32, 0, NULL);
  memset(frames, 0, nb_frames / 32);

  /* allocate kernel pgd */
  kernel_pgd = (struct page_directory_t *) kmalloc(sizeof(struct page_directory_t), 1, NULL);
  memset(kernel_pgd, 0, sizeof(struct page_directory_t));
  current_pgd = kernel_pgd;

  /* allocate frames and pages */
  for (i = 0; i < placement_address; i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* register page fault handler */
  register_interrupt_handler(14, page_fault_handler);

  /* enable paging */
  switch_page_directory(current_pgd);
}

/*
 * Switch page directory.
 */
void switch_page_directory(struct page_directory_t *pgd)
{
  uint32_t cr0;

  /* switch */
  current_pgd = pgd;
  __asm__ volatile("mov %0, %%cr3" :: "r" (&pgd->tables_physical));
  __asm__ volatile("mov %%cr0, %0" : "=r" (cr0));

  /* enable paging */
  cr0 |= 0x80000000;
  __asm__ volatile("mov %0, %%cr0" :: "r" (cr0));
}

/*
 * Get page from pgd at virtual address. If make is set and the page doesn't exist, create it.
 */
struct page_t *get_page(uint32_t address, uint8_t make, struct page_directory_t *pgd)
{
  uint32_t table_idx;
  uint32_t tmp;

  /* get page table */
  address /= PAGE_SIZE;
  table_idx = address / 1024;

  /* table already assigned */
  if (pgd->tables[table_idx])
    return &pgd->tables[table_idx]->pages[address % 1024];

  /* create a new page table */
  if (make) {
    pgd->tables[table_idx] = (struct page_table_t *) kmalloc(sizeof(struct page_table_t), 1, &tmp);
    memset(pgd->tables[table_idx], 0, 0x1000);
    pgd->tables_physical[table_idx] = tmp | 0x7; /* present, rw and user */
    return &pgd->tables[table_idx]->pages[address % 1024];
  }

  return 0;
}
