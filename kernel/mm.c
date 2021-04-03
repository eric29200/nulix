#include <kernel/mm.h>
#include <kernel/mm_paging.h>
#include <kernel/mm_heap.h>
#include <lib/string.h>

/* placement address (used before kernel heap is created) */
uint32_t placement_address = 0;

/* kernel heap */
struct heap_t *kheap = NULL;

/*
 * Allocate memory.
 */
uint32_t kmalloc_phys(uint32_t size, uint8_t align, uint32_t *phys)
{
  uint32_t ret;

  /* align adress on PAGE boundary */
  if (align == 1 && !PAGE_ALIGNED(placement_address))
    placement_address = PAGE_ALIGN(placement_address);

  /* get physical memory */
  if (phys)
    *phys = placement_address;

  /* update placement_address */
  ret = placement_address;
  placement_address += size;
  return ret;
}

/*
 * Allocate memory on the kernel heap.
 */
void *kmalloc(uint32_t size)
{
  if (kheap)
    return heap_alloc(kheap, size);

  return (void *) kmalloc_phys(size, 0, NULL);
}

/*
 * Free memory on the kernel heap.
 */
void kfree(void *p)
{
  if (kheap)
    heap_free(kheap, p);
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
  frames = (uint32_t *) kmalloc(nb_frames / 32);
  memset(frames, 0, nb_frames / 32);

  /* allocate kernel page directory */
  kernel_pgd = (struct page_directory_t *) kmalloc_phys(sizeof(struct page_directory_t), 1, NULL);
  memset(kernel_pgd, 0, sizeof(struct page_directory_t));
  current_pgd = kernel_pgd;

  /* map heap frames */
  for (i = KHEAP_START; i < KHEAP_START + KHEAP_INIT_SIZE; i += PAGE_SIZE)
    get_page(i, 1, kernel_pgd);

  /* allocate kernel frames and pages (maximum size of kernel page tables has to be added) */
  max_nb_page_tables = nb_frames / 1024;
  for (i = 0; i <= placement_address + max_nb_page_tables * sizeof(struct page_table_t); i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* allocate heap frames */
  for (i = KHEAP_START; i < KHEAP_START + KHEAP_INIT_SIZE; i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* register page fault handler */
  register_interrupt_handler(14, page_fault_handler);

  /* enable paging */
  switch_page_directory(current_pgd);

  /* init heap */
  kheap = heap_create(KHEAP_START, KHEAP_START + end, KHEAP_INIT_SIZE);
}
