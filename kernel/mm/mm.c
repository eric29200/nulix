#include <mm/mm.h>
#include <mm/paging.h>
#include <mm/heap.h>
#include <string.h>
#include <stdio.h>

/* placement address (used before kernel heap is created) */
uint32_t placement_address = 0;

/* kernel heap */
struct heap_t kheap;

/*
 * Allocate memory.
 */
void *kmalloc(uint32_t size)
{
  return heap_alloc(&kheap, size, 0);
}

/*
 * Allocate page aligned memory.
 */
void *kmalloc_align(uint32_t size)
{
  return heap_alloc(&kheap, size, 1);
}

/*
 * Allocate memory in low memory.
 */
void *kmalloc_phys(uint32_t size, uint8_t align, uint32_t *phys)
{
  void *ret;

  /* align adress on PAGE boundary */
  if (align == 1 && !PAGE_ALIGNED(placement_address))
    placement_address = PAGE_ALIGN(placement_address);

  /* get physical memory */
  if (phys)
    *phys = placement_address;

  /* update placement_address */
  ret = (void *) placement_address;
  placement_address += size;

  return ret;
}

/*
 * Free memory on the kernel heap.
 */
void kfree(void *p)
{
  heap_free(&kheap, p);
}

/*
 * Init memory paging and kernel heap.
 */
void init_mem(uint32_t start, uint32_t end)
{
  uint32_t max_nb_page_tables;
  uint32_t i;

  /* set placement address */
  placement_address = start;

  /* set frames */
  nb_frames = end / PAGE_SIZE;
  frames = (uint32_t *) kmalloc_phys(nb_frames / 32, 0, NULL);
  memset(frames, 0, nb_frames / 32);

  /* allocate kernel page directory */
  kernel_pgd = (struct page_directory_t *) kmalloc_phys(sizeof(struct page_directory_t), 1, NULL);
  memset(kernel_pgd, 0, sizeof(struct page_directory_t));
  kernel_pgd->physical_addr = (uint32_t) kernel_pgd->tables_physical;

  /* allocate kernel frames/pages + kernel pages tables frames/pages */
  max_nb_page_tables = nb_frames / 1024;
  for (i = 0; i <= placement_address + PAGE_SIZE + max_nb_page_tables * sizeof(struct page_table_t); i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* register page fault handler */
  register_interrupt_handler(14, page_fault_handler);

  /* enable paging */
  switch_page_directory(kernel_pgd);

  /* allocate heap frames */
  for (i = KHEAP_START; i < KHEAP_START + KHEAP_SIZE; i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* init heap */
  if (heap_init(&kheap, KHEAP_START, KHEAP_SIZE) != 0)
    panic("Cannot create kernel heap");
}
