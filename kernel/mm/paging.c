#include <mm/paging.h>
#include <mm/mm.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>

/* bit frames */
uint32_t *frames;
uint32_t nb_frames;

/* page directories */
struct page_directory_t *kernel_pgd = 0;

/* copy phsyical page (defined in x86/paging.s) */
extern void copy_page_physical(uint32_t src, uint32_t dst);

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
void page_fault_handler(struct registers_t *regs)
{
  uint32_t fault_addr;

  /* faulting address is stored in CR2 register */
  __asm__ volatile("mov %%cr2, %0" : "=r" (fault_addr));

  /* get erros informations */
  int present = !(regs->err_code & 0x1);
  int rw = regs->err_code & 0x2;
  int user = regs->err_code & 0x4;
  int reserved = regs->err_code & 0x8;
  int id = regs->err_code & 0x10;

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
  __asm__ volatile("mov %0, %%cr3" :: "r" (pgd->tables_physical));
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
    if (!src->pages[i].frame)
      continue;

    alloc_frame(&ret->pages[i], 0, 0);
    ret->pages[i].present = src->pages[i].present;
    ret->pages[i].rw = src->pages[i].rw;
    ret->pages[i].user = src->pages[i].user;
    ret->pages[i].accessed = src->pages[i].accessed;
    ret->pages[i].dirty = src->pages[i].dirty;
    copy_page_physical(src->pages[i].frame * PAGE_SIZE, ret->pages[i].frame * PAGE_SIZE);
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
      if (ret->tables[i])
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
    if (pgt->pages[i].frame)
      free_frame(&pgt->pages[i]);

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
  for (i = 0; i < 1024; i++) {
    /* do not free kernel page tables */
    if (!pgd->tables[i] || pgd->tables[i] == kernel_pgd->tables[i])
      continue;

    free_page_table(pgd->tables[i]);
  }

  /* free page directory */
  kfree(pgd);
}
