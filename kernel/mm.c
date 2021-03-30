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
  struct page_t *page;
  uint32_t ret;

  /* use kernel heap */
  if (kheap) {
    void *addr = heap_alloc(kheap, size, align);
    if (phys) {
      page = get_page((uint32_t) addr, 0, kernel_pgd);
      *phys = page->frame * PAGE_SIZE + ((uint32_t) addr & 0xFFF);
    }

    return (uint32_t) addr;
  }

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
 * Allocate memory.
 */
void *kmalloc(uint32_t size)
{
  return (void *) kmalloc_phys(size, 0, NULL);
}

/*
 * Free memory.
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
  uint32_t max_size_of_page_tables;
  uint32_t i;

  /* set placement address (some memory is reserved for the heap structure) */
  placement_address = start + 0x100;

  /* set frames */
  nb_frames = end / PAGE_SIZE;
  frames = (uint32_t *) kmalloc(nb_frames / 32);
  memset(frames, 0, nb_frames / 32);

  /* allocate kernel pgd */
  kernel_pgd = (struct page_directory_t *) kmalloc_phys(sizeof(struct page_directory_t), 1, NULL);
  memset(kernel_pgd, 0, sizeof(struct page_directory_t));
  current_pgd = kernel_pgd;

  /* map heap pages */
  for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += PAGE_SIZE)
    get_page(i, 1, kernel_pgd);

  /* allocate kernel frames and pages (maximum size of kernel page tables has to be added) */
  max_size_of_page_tables =  (nb_frames / 1024) * sizeof(struct page_table_t);
  for (i = 0; i < placement_address + max_size_of_page_tables; i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* allocate heap frames */
  for (i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* register page fault handler */
  register_interrupt_handler(14, page_fault_handler);

  /* enable paging */
  switch_page_directory(current_pgd);

  /* init heap */
  kheap = create_heap(KHEAP_START, KHEAP_START + KHEAP_INITIAL_SIZE, KHEAP_START + end, 0, 0);
}