#ifndef _MM_HEAP_H_
#define _MM_HEAP_H_

#include <lib/stddef.h>
#include <lib/ordered_array.h>

#define KHEAP_START             0xC0000000
#define KHEAP_INITIAL_SIZE      0x100000
#define HEAP_MIN_SIZE           0x70000
#define HEAP_INDEX_SIZE         0x20000
#define HEAP_MAGIC              0x123890AB

/*
 * Header of a memory block.
 */
struct mm_header_t {
  uint32_t magic;
  uint8_t is_hole;
  uint32_t size;
};

/*
 * Footer of a memory block.
 */
struct mm_footer_t {
  uint32_t magic;
  struct mm_header_t *header;
};

/*
 * Heap structure.
 */
struct heap_t {
  struct ordered_array_t index;
  uint32_t start_address;
  uint32_t end_address;
  uint32_t max_address;
  uint8_t supervisor;
  uint8_t readonly;
};

struct heap_t *create_heap(uint32_t start_addr, uint32_t end_addr, uint32_t max_addr, uint8_t supervisor, uint8_t readonly);
void *heap_alloc(struct heap_t *heap, uint32_t size, uint8_t page_align);
void heap_free(struct heap_t *heap, void *p);

#endif
