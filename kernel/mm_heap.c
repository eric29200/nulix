#include <kernel/mm_heap.h>
#include <kernel/mm_paging.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* kernel heap */
struct heap_t *kheap;

/*
 * Create a heap.
 */
void init_kheap(uint32_t start_addr, uint32_t end_addr, uint32_t max_addr, uint8_t supervisor, uint8_t readonly)
{
  struct mm_header_t *start_header;
  struct mm_header_t *end_header;

  /* align start address on page boundary */
  if ((start_addr & 0xFFFFF000) != 0) {
    start_addr &= 0xFFFFF000;
    start_addr += 0x1000;
  }

  /* set start/end of heap */
  start_header = (struct mm_header_t *) start_addr;
  end_header = (struct mm_header_t *) (end_addr - sizeof(struct mm_header_t));

  /* set start header */
  start_header->next = end_header;
  start_header->prev = end_header;
  strncpy(start_header->name, "start", sizeof(start_header->name) - 1);
  start_header->size = 0;

  /* set end header */
  end_header->next = start_header;
  end_header->prev = start_header;
  strncpy(end_header->name, "end", sizeof(end_header->name) - 1);
  end_header->size = 0;

  kheap->start = start_header;
  kheap->end = end_header;
  kheap->max_addr = max_addr;
  kheap->supervisor = supervisor;
  kheap->readonly = readonly;
}

/*
 * Get size of a heap.
 */
static uint32_t heap_size(struct heap_t *heap)
{
  return (uint32_t) heap->end + sizeof(struct mm_header_t) - (uint32_t) heap->start + 1;
}

/*
 * Expand a heap.
 */
static void heap_expand(struct heap_t *heap, uint32_t new_size)
{
  struct mm_header_t *end_header, *tmp;
  uint32_t old_size;
  uint32_t i;

  /* no need to expand */
  old_size = heap_size(heap);
  if (new_size <= old_size)
    return;

  /* align new size on a boundary page */
  if ((new_size & 0xFFFFF000) != 0) {
    new_size &= 0xFFFFF000;
    new_size += PAGE_SIZE;
  }

  /* heap overflow */
  if ((uint32_t) heap->start + new_size > heap->max_addr)
    printf("Heap expansion overflow\n");

  /* allocate frames and pages */
  for (i = old_size; i <= new_size; i += PAGE_SIZE)
    alloc_frame(get_page((uint32_t) heap->start + i, 1, kernel_pgd), heap->supervisor ? 1 : 0, heap->readonly ? 0 : 1);

  /* update blocks linked list */
  end_header = (struct mm_header_t *) ((uint32_t) heap->start + new_size - sizeof(struct mm_header_t));
  tmp = heap->end->prev;
  tmp->next = end_header;
  heap->end = end_header;
  heap->end->next = heap->start;
  heap->end->prev = tmp;
  strncpy(heap->end->name, "end", sizeof(heap->end->name) - 1);
  heap->end->size = 0;
  heap->start->prev = heap->end;
  heap->start->size = 0;
}
