#include <kernel/mm_heap.h>
#include <kernel/mm.h>

/*
 * Create a heap.
 */
struct heap_t *heap_create(uint32_t start_address, uint32_t end_address)
{
  struct heap_t *heap;

  /* check heap size is big enough */
  if (end_address - start_address <= sizeof(struct heap_block_t))
    return NULL;

  /* allocate a new heap */
  heap = (struct heap_t *) kmalloc_phys(sizeof(struct heap_t), 0, 0);
  if (!heap)
    return NULL;

  /* set start/end address */
  heap->first_block = (struct heap_block_t *) start_address;
  heap->end_address = end_address;

  /* create first block */
  heap->first_block->size = end_address - start_address - sizeof(struct heap_block_t);
  heap->first_block->free = 1;
  heap->first_block->prev = NULL;
  heap->first_block->next = NULL;

  return heap;
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
  if (!block)
    return NULL;

  /* create new free block with remaining size */
  if (block->size - size > sizeof(struct heap_block_t)) {
    /* create new free block */
    new_free_block = (struct heap_block_t *) ((uint32_t) block + sizeof(struct heap_block_t) + size);
    new_free_block->size = block->size - size - sizeof(struct heap_block_t);
    new_free_block->free = 1;
    new_free_block->prev = block;
    new_free_block->next = block->next;

    /* update this block */
    block->size = size;
    block->next = new_free_block;
  }

  /* mark this block */
  block->free = 0;

  return (void *) ((uint32_t) block + sizeof(struct heap_block_t));
}

/*
 * Free memory on the heap.
 */
void heap_free(void *p)
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

    if (block->next->next != NULL)
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
