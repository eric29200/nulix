#include <mm/heap.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>

#define HEAP_BLOCK_DATA(block)          ((uint32_t) (block) + sizeof(struct heap_block_t))
#define HEAP_BLOCK_ALIGNED(block)       (PAGE_ALIGNED(HEAP_BLOCK_DATA(block)))

/*
 * Create a heap.
 */
struct heap_t *heap_create(uint32_t start_address, size_t size)
{
  struct heap_t *heap;

  /* check heap size */
  if (size <= sizeof(struct heap_block_t))
    return NULL;

  /* allocate new heap */
  heap = kmalloc(sizeof(struct heap_t));
  if (!heap)
    return NULL;

  /* set start/end addresses */
  heap->first_block = (struct heap_block_t *) start_address;
  heap->start_address = start_address;
  heap->end_address = start_address + size;
  heap->size = size;

  /* create first block */
  heap->first_block->size = size - sizeof(struct heap_block_t);
  heap->first_block->free = 1;
  heap->first_block->prev = NULL;
  heap->first_block->next = NULL;
  heap->last_block = heap->first_block;

  return heap;
}

/*
 * Find a free block.
 */
static struct heap_block_t *heap_find_free_block(struct heap_t *heap, uint8_t page_aligned, size_t size)
{
  struct heap_block_t *block;
  uint32_t page_offset;

  for (block = heap->first_block; block != NULL; block = block->next) {
    /* skip busy blocks */
    if (!block->free)
      continue;

    /* compute page offset */
    page_offset = 0;
    if (page_aligned && !HEAP_BLOCK_ALIGNED(block))
      page_offset = PAGE_SIZE - HEAP_BLOCK_DATA(block) % PAGE_SIZE;

    /* check size */
    if (block->size >= size + page_offset)
      return block;
  }

  return NULL;
}

/*
 * Allocate memory on the heap.
 */
void *heap_alloc(struct heap_t *heap, size_t size, uint8_t page_aligned)
{
  struct heap_block_t *block, *new_block;
  uint32_t page_offset;

  /* find free block */
  block = heap_find_free_block(heap, page_aligned, size);
  if (!block)
    return NULL;

  /* if page alignement is asked, move block */
  if (page_aligned && !HEAP_BLOCK_ALIGNED(block)) {
    /* compute page offset */
    page_offset = PAGE_SIZE - HEAP_BLOCK_DATA(block) % PAGE_SIZE;

    /* move block */
    new_block = (void *) block + page_offset;
    new_block->size = block->size - page_offset;
    new_block->prev = block->prev;
    new_block->next = block->next;
    block = new_block;

    /* update previous block size */
    if (block->prev) {
      block->prev->size += page_offset;
      block->prev->next = block;
    } else {
      heap->first_block = new_block;
    }

    /* update next block */
    if (block->next)
      block->next->prev = block;
    else
      heap->last_block = new_block;
  }

  /* create new last free block with remaining size */
  if (block == heap->last_block && block->size - size > sizeof(struct heap_block_t)) {
    /* create new free block */
    new_block = (struct heap_block_t *) (HEAP_BLOCK_DATA(block) + size);
    new_block->size = block->size - size - sizeof(struct heap_block_t);
    new_block->free = 1;
    new_block->prev = block;
    new_block->next = NULL;

    /* update this block */
    block->size = size;
    block->next = new_block;

    /* update heap last block */
    heap->last_block = new_block;
  }

  /* mark this block */
  block->free = 0;

  return (void *) HEAP_BLOCK_DATA(block);
}

/*
 * Free memory on the heap.
 */
void heap_free(struct heap_t *heap, void *p)
{
  struct heap_block_t *block;

  /* do not free NULL */
  if (!p)
    return;

  /* mark block as free */
  block = (struct heap_block_t *) ((uint32_t) p - sizeof(struct heap_block_t));
  block->free = 1;

  /* merge with right block */
  if (block->next && block->next->free) {
    block->size += block->next->size + sizeof(struct heap_block_t);
    block->next = block->next->next;

    if (block->next)
      block->next->prev = block;
    else
      heap->last_block = block;
  }

  /* merge with left block */
  if (block->prev && block->prev->free) {
    block->prev->size += block->size + sizeof(struct heap_block_t);
    block->prev->next = block->next;

    if (block->next)
      block->next->prev = block->prev;
    else
      heap->last_block = block->prev;
  }
}

/*
 * Dump the heap on the screen.
 */
void heap_dump(struct heap_t *heap)
{
  struct heap_block_t *block;

  for (block = heap->first_block; block != NULL; block = block->next)
    printf("%x\t%d\t%d\n", (uint32_t) block, block->size, block->free);
}
