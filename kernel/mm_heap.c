#include <kernel/mm_heap.h>
#include <kernel/mm_paging.h>
#include <kernel/mm.h>
#include <lib/stderr.h>

#define HEAP_BLOCK_DATA(block)          ((uint32_t) (block) + sizeof(struct heap_block_t))

/*
 * Create a heap.
 */
struct heap_t *heap_create(uint32_t start_address, uint32_t max_address, size_t size)
{
  struct heap_t *heap;

  /* check heap size */
  if (size <= sizeof(struct heap_block_t) || size > (max_address - start_address))
    return NULL;

  /* allocate a new heap */
  heap = (struct heap_t *) kmalloc(sizeof(struct heap_t));
  if (!heap)
    return NULL;

  /* set start/end addresses */
  heap->first_block = (struct heap_block_t *) start_address;
  heap->start_address = start_address;
  heap->end_address = start_address + size;
  heap->max_address = max_address;
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
 * Expand a heap.
 */
static int heap_expand(struct heap_t *heap, size_t new_size)
{
  struct heap_block_t *new_last_block;
  uint32_t i;

  /* always add sizeof(heap_block) */
  new_size += sizeof(struct heap_block_t);

  /* no need to expand */
  if (new_size <= heap->size)
    return 0;

  /* always increment heap by step of HEAP_EXPANSION (for performance) */
  new_size = ALIGN_UP(new_size, HEAP_EXPANSION_SIZE);

  /* out of memory */
  if (heap->start_address + new_size > heap->max_address)
    return ENOMEM;

  /* allocate new pages */
  for (i = heap->end_address; i < heap->start_address + new_size + PAGE_SIZE; i += PAGE_SIZE)
    alloc_frame(get_page(i, 1, current_pgd), 1, 1);

  /* extend last block */
  if (heap->last_block->free) {
    heap->last_block->size += new_size - heap->size;
    goto out;
  }

  /* create new last block */
  new_last_block = (struct heap_block_t *) heap->end_address;
  new_last_block->free = 1;
  new_last_block->size = new_size - heap->size - sizeof(struct heap_block_t);
  new_last_block->prev = heap->last_block;
  new_last_block->next = NULL;

  /* update heap */
  heap->last_block->next = new_last_block;
  heap->last_block = new_last_block;

out:
  /* update heap size */
  heap->size = new_size;
  heap->end_address = heap->start_address + heap->size;
  return 0;
}

/*
 * Find a free block.
 */
static struct heap_block_t *heap_find_free_block(struct heap_t *heap, size_t size)
{
  struct heap_block_t *block;

  for (block = heap->first_block; block != NULL; block = block->next)
    if (block->free && block->size >= size)
      return block;

  return NULL;
}

/*
 * Allocate memory on the heap.
 */
void *heap_alloc(struct heap_t *heap, uint32_t size)
{
  struct heap_block_t *block, *new_free_block;

  /* find free block */
  block = heap_find_free_block(heap, size);
  if (!block) {
    /* try to expand the heap */
    if (heap_expand(heap, heap->size + size) != 0)
      return NULL;

    /* retry with extanded heap */
    return heap_alloc(heap, size);
  }

  /* create new free block with remaining size */
  if (block->size - size > sizeof(struct heap_block_t)) {
    /* create new free block */
    new_free_block = (struct heap_block_t *) (HEAP_BLOCK_DATA(block) + size);
    new_free_block->size = block->size - size - sizeof(struct heap_block_t);
    new_free_block->free = 1;
    new_free_block->prev = block;
    new_free_block->next = block->next;

    /* update this block */
    block->size = size;
    block->next = new_free_block;

    /* update heap last block if needed */
    if (new_free_block->next == NULL)
      heap->last_block = new_free_block;
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

  /* check if block can be merged on the right */
  if (block->next != NULL && block->next->free) {
    block->size += block->next->size + sizeof(struct heap_block_t);

    if (block->next->next == NULL)
      heap->last_block = block;
    else
      block->next->next->prev = block;

    block->next = block->next->next;
  }

  /* check if block can be merged on the left */
  if (block->prev != NULL && block->prev->free) {
    block->prev->next = block->next;

    if (block->next != NULL)
      block->next->prev = block->prev;

    block->prev->size += block->size + sizeof(struct heap_block_t);
  }
}
