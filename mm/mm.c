#include <mm/mm.h>
#include <mm/paging.h>
#include <mm/heap.h>
#include <string.h>

/* placement address (used before kernel heap is created) */
uint32_t placement_address = 0;

/* kernel heap */
struct heap_t *kheap = NULL;

/*
 * Allocate memory (internal function).
 */
static void *__kmalloc(uint32_t size, uint8_t align, uint32_t *phys)
{
  struct page_t *page;
  void *ret;

  /* use kernel heap */
  if (kheap) {
    ret = heap_alloc(kheap, size, align);

    if (phys) {
      page = get_page((uint32_t) ret, 0, kernel_pgd);
      *phys = page->frame * PAGE_SIZE + ((uint32_t) ret & 0xFFF);
    }

    return ret;
  }

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
 * Allocate memory.
 */
void *kmalloc(uint32_t size)
{
  return __kmalloc(size, 0, NULL);
}

/*
 * Allocate page aligned memory.
 */
void *kmalloc_align(uint32_t size)
{
  return __kmalloc(size, 1, NULL);
}

/*
 * Allocate page aligned memory and output physical memory.
 */
void *kmalloc_align_phys(uint32_t size, uint32_t *phys)
{
  return __kmalloc(size, 1, phys);
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
  uint32_t max_heap_size;
  uint32_t i;

  /* set placement address */
  placement_address = start;

  /* set frames */
  nb_frames = end / PAGE_SIZE;
  frames = (uint32_t *) kmalloc(nb_frames / 32);
  memset(frames, 0, nb_frames / 32);

  /* allocate kernel page directory */
  kernel_pgd = (struct page_directory_t *) kmalloc_align(sizeof(struct page_directory_t));
  memset(kernel_pgd, 0, sizeof(struct page_directory_t));
  kernel_pgd->physical_addr = (uint32_t) kernel_pgd->tables_physical;

  /* allocate kernel frames/pages + kernel pages tables frames/pages */
  max_nb_page_tables = nb_frames / 1024;
  for (i = 0; i <= placement_address + PAGE_SIZE + max_nb_page_tables * sizeof(struct page_table_t); i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* register page fault handler */
  register_interrupt_handler(14, page_fault_handler);

  /* enable paging */
  current_pgd = kernel_pgd;
  switch_page_directory(current_pgd);

  /* compute max heap size (all memory / 2) */
  max_heap_size = (end - placement_address) / 2;
  if (max_heap_size < KHEAP_INIT_SIZE)
    max_heap_size = KHEAP_INIT_SIZE;

  /* map all heap frames */
  for (i = KHEAP_START; i < KHEAP_START + max_heap_size; i += PAGE_SIZE)
    get_page(i, 1, kernel_pgd);

  /* allocate initial heap frames */
  for (i = KHEAP_START; i < KHEAP_START + KHEAP_INIT_SIZE; i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, kernel_pgd), 0, 0);

  /* init heap */
  kheap = heap_create(KHEAP_START, KHEAP_START + max_heap_size, KHEAP_INIT_SIZE);
}
