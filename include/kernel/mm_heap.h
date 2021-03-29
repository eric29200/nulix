#ifndef _MM_HEAP_H_
#define _MM_HEAP_H_

#include <lib/stddef.h>

#define KHEAP_START             0xC0000000
#define KHEAP_INITIAL_SIZE      0x500000
#define HEAP_MIN_SIZE           0x70000

/*
 * Header of a memory block.
 */
struct mm_header_t {
  struct mm_header_t *prev;
  struct mm_header_t *next;
  char name[32];
  uint32_t size;
};

/*
 * Heap structure.
 */
struct heap_t {
  struct mm_header_t *start;
  struct mm_header_t *end;
  uint32_t max_addr;
  uint8_t supervisor;
  uint8_t readonly;
};

void init_kheap(uint32_t start_addr, uint32_t end_addr, uint32_t max_addr, uint8_t supervisor, uint8_t readonly);

#endif
