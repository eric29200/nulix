#include <mm/heap.h>
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
