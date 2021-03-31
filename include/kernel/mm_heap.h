#ifndef _MM_HEAP_H_
#define _MM_HEAP_H_

#include <lib/stddef.h>

/*
 * Heap block header.
 */
struct heap_block_t {
  uint32_t size;
  uint8_t free;
  struct heap_block_t *prev;
  struct heap_block_t *next;
};

/*
 * Heap structure.
 */
struct heap_t {
  struct heap_block_t *first_block;
  uint32_t end_address;
};

struct heap_t *heap_create(uint32_t start_address, uint32_t end_address);
void *heap_alloc(struct heap_t *heap, uint32_t size);
void heap_free(void *p);

#endif
