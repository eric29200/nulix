#include <mm/heap.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <stderr.h>

#define HEAP_BLOCK_DATA(block)          ((uint32_t) (block) + sizeof(struct heap_block_t))
#define HEAP_BLOCK_ALIGNED(block)       (PAGE_ALIGNED(HEAP_BLOCK_DATA(block)))

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
  spin_lock_init(&heap->lock);

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
    alloc_frame(get_page(i, 1, kernel_pgd), 1, 1);

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
  struct heap_block_t *block, *aligned_block, *new_free_block;
  uint32_t page_offset, new_size, flags;

  /* lock the heap */
  spin_lock_irqsave(&heap->lock, flags);

  /* find free block */
  block = heap_find_free_block(heap, page_aligned, size);
  if (!block) {
    /* expand the heap (if page alignement is asked, add PAGE_SIZE to be sure) */
    new_size = heap->size + size + (page_aligned ? PAGE_SIZE : 0);

    /* if last block is free, remove last block size */
    if (heap->last_block->free)
      new_size -= heap->last_block->size;

    /* try to expand */
    if (heap_expand(heap, new_size) != 0) {
      spin_unlock_irqrestore(&heap->lock, flags);
      return NULL;
    }

    /* retry with extanded heap */
    spin_unlock_irqrestore(&heap->lock, flags);
    return heap_alloc(heap, size, page_aligned);
  }

  /* if page alignement is asked, create a new hole in front of the page aligned block */
  if (page_aligned && !HEAP_BLOCK_ALIGNED(block)) {
    /* compute page offset */
    page_offset = PAGE_SIZE - HEAP_BLOCK_DATA(block) % PAGE_SIZE;

    /* create a block on page alignement */
    aligned_block = (struct heap_block_t *) ((uint32_t) block + page_offset);
    aligned_block->size = block->size - page_offset;
    aligned_block->free = 1;
    aligned_block->prev = block;
    aligned_block->next = block->next;

    /* update current block with remaining space */
    block->size = (uint32_t) aligned_block - HEAP_BLOCK_DATA(block);
    block->free = 1;
    block->next = aligned_block;

    /* this block = aligned block */
    block = aligned_block;
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

  spin_unlock_irqrestore(&heap->lock, flags);
  return (void *) HEAP_BLOCK_DATA(block);
}

/*
 * Free memory on the heap.
 */
void heap_free(struct heap_t *heap, void *p)
{
  struct heap_block_t *block;
  uint32_t flags;

  /* do not free NULL */
  if (!p)
    return;

  /* mark block as free */
  spin_lock_irqsave(&heap->lock, flags);
  block = (struct heap_block_t *) ((uint32_t) p - sizeof(struct heap_block_t));
  block->free = 1;
  spin_unlock_irqrestore(&heap->lock, flags);
}