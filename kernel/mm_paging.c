#include <kernel/mm_paging.h>
#include <kernel/mm.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <lib/stderr.h>

/* bit frames */
uint32_t *frames;
uint32_t nb_frames;

/* page directories */
struct page_directory_t *kernel_pgd = 0;
struct page_directory_t *current_pgd = 0;

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
int32_t alloc_frame(struct page_t *page, uint8_t kernel, uint8_t write)
{
  int32_t free_frame_idx;

  /* frame already allocated */
  if (page->frame != 0)
    return 0;

  /* get a new frame */
  free_frame_idx = get_first_free_frame();
  if (free_frame_idx < 0)
    return ENOMEM;

  set_frame(PAGE_SIZE * free_frame_idx);
  page->present = 1;
  page->frame = free_frame_idx;
  page->rw = write ? 1 : 0;
  page->user = kernel ? 0 : 1;

  return 0;
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
  uint32_t frame;

  if (!(frame = page->frame))
    return;

  clear_frame(frame);
  page->frame = 0x0;
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
  printf("Page fault at address=%x | present=%d read-only=%d user-mode=%d reserved=%d instruction-fetch=%d\n",
         fault_addr, present, rw, user, reserved, id);
  panic("");
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
    pgd->tables[table_idx] = (struct page_table_t *) kmalloc_align_phys(sizeof(struct page_table_t), &tmp);
    memset(pgd->tables[table_idx], 0, PAGE_SIZE);
    pgd->tables_physical[table_idx] = tmp | 0x7; /* present, rw and user */
    return &pgd->tables[table_idx]->pages[address % 1024];
  }


  return 0;
}
