#include <mm/mem.h>
#include <mm/paging.h>
#include <mm/heap.h>
#include <lib/string.h>

/*
 * Init memory paging and kernel heap.
 */
void init_mem(uint32_t start, uint32_t end)
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

  /* allocate kernel frames and pages */
  for (i = 0; i < placement_address; i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* allocate frames and pages of the heap */
  for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* register page fault handler */
  register_interrupt_handler(14, page_fault_handler);

  /* enable paging */
  switch_page_directory(current_pgd);

  /* init heap */
  init_kheap(KHEAP_START, KHEAP_START + KHEAP_INITIAL_SIZE, KHEAP_START + end, 0, 0);
}
