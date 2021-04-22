#ifndef _MM_HEAP_H_
#define _MM_HEAP_H_

#include <stddef.h>
#include <lock.h>

/*
 * Heap block header.
 */
struct heap_block_t {
  uint32_t size;
  uint8_t free;
  struct heap_block_t *prev;
  struct heap_block_t *next;
} __attribute__((packed));

/*
 * Heap structure.
 */
struct heap_t {
  struct heap_block_t *first_block;
  struct heap_block_t *last_block;
  uint32_t start_address;
  uint32_t end_address;
  size_t size;
  spinlock_t lock;
} __attribute__((packed));

int heap_init(struct heap_t *heap, uint32_t start_address, size_t size);
void *heap_alloc(struct heap_t *heap, size_t size, uint8_t page_aligned);
void heap_free(struct heap_t *heap, void *p);

#endif
