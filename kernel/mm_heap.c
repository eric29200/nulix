#include <kernel/mm_heap.h>
#include <kernel/mm_paging.h>
#include <kernel/mm.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* kernel heap */
struct heap_t *kheap;

/*
 * Block header comparison.
 */
static int8_t header_cmp(void *a, void *b)
{
  return (((struct mm_header_t *) a)->size < ((struct mm_header_t *) b)->size) ? 1 : 0;
}

/*
 * Create a heap.
 */
struct heap_t *create_heap(uint32_t start_addr, uint32_t end_addr, uint32_t max_addr, uint8_t supervisor, uint8_t readonly)
{
  struct mm_header_t *first_item;
  struct heap_t *heap;

  /* allocate new heap */
  heap = (struct heap_t *) kmalloc(sizeof(struct heap_t));

  /* init items */
  heap->index = ordered_array_create((void *) start_addr, HEAP_INDEX_SIZE, &header_cmp);

  /* skip the ordered array index */
  start_addr += sizeof(void *) * HEAP_INDEX_SIZE;

  /* start address must be on a boundary page */
  if ((start_addr & 0xFFFFF000) != 0) {
    start_addr &= 0xFFFFF000;
    start_addr += PAGE_SIZE;
  }

  /* set heap */
  heap->start_address = start_addr;
  heap->end_address = end_addr;
  heap->max_address = max_addr;
  heap->supervisor = supervisor;
  heap->readonly = readonly;

  /* create and insert first item */
  first_item = (struct mm_header_t *) start_addr;
  first_item->size = end_addr - start_addr;
  first_item->magic = HEAP_MAGIC;
  first_item->is_hole = 1;
  ordered_array_insert(&heap->index, (void *) first_item);

  return heap;
}

/*
 * Expand a heap.
 */
static void expand(struct heap_t *heap, uint32_t new_size)
{
  uint32_t i;

  /* no need to expand */
  if (new_size <= heap->end_address - heap->start_address)
    return;

  /* align on page boundary */
  if ((new_size & 0xFFFFF000) != 0) {
    new_size &= 0xFFFFF000;
    new_size += PAGE_SIZE;
  }

  /* check overflow */
  if (heap->start_address + new_size > heap->max_address)
    printf("[Kernel] Heap expansion overflow\n");

  /* allocate new frames */
  for (i = heap->end_address - heap->start_address; i < new_size; i += PAGE_SIZE)
    alloc_frame(get_page(heap->start_address + i, 1, kernel_pgd), heap->supervisor ? 1 : 0, heap->readonly ? 1 : 0);

  /* update end address */
  heap->end_address = heap->start_address + new_size;
}

/*
 * Find smallest hole matching size.
 */
static int32_t find_smallest_hole(struct heap_t *heap, uint32_t size, uint8_t page_align)
{
  struct mm_header_t *header;
  int32_t offset = 0;
  uint32_t i;

  for (i = 0; i < heap->index.size; i++) {
    header = (struct mm_header_t *) heap->index.items[i];

    if (page_align > 0) {
      if ((((uint32_t) header + sizeof(struct mm_header_t)) & 0xFFFFF) != 0)
        offset = PAGE_SIZE;

      if (header->size - offset >= size)
        break;
    } else if (header->size >= size) {
      break;
    }
  }

  /* no matching hole */
  if (i == heap->index.size)
    return -1;

  return i;
}

/*
 * Allocate memory on the heap.
 */
void *heap_alloc(struct heap_t *heap, uint32_t size, uint8_t page_align)
{
  uint32_t new_size;
  int32_t i;
  uint32_t j;

  /* add header and footer to size */
  new_size = size + sizeof(struct mm_header_t) + sizeof(struct mm_footer_t);

  /* find smallest hole */
  i = find_smallest_hole(heap, new_size, page_align);

  /* no matching hole */
  if (i == -1) {
    /* expand the heap */
    uint32_t old_length = heap->end_address - heap->start_address;
    uint32_t old_end_address = heap->end_address;
    expand(heap, old_length + new_size);

    /* find last item (in location address) */
    int32_t last_item_idx = -1;
    uint32_t last_item_addr = 0x0;
    for (j = 0; j < heap->index.size; j++) {
      if ((uint32_t) heap->index.items[j] > last_item_addr) {
        last_item_addr = (uint32_t) heap->index.items[j];
        last_item_idx = j;
      }
    }

    /* no item in the heap : create new one */
    if (last_item_idx == -1) {
      /* create header */
      struct mm_header_t *header = (struct mm_header_t *) old_end_address;
      header->magic = HEAP_MAGIC;
      header->size = heap->end_address - heap->start_address - old_length;
      header->is_hole = 1;

      /* create footer */
      struct mm_footer_t *footer = (struct mm_footer_t *) (old_end_address + header->size - sizeof(struct mm_footer_t));
      footer->magic = HEAP_MAGIC;
      footer->header = header;

      /* add to the heap */
      ordered_array_insert((void *) header, &heap->index);

      /* we should have enough space now : recall the alloc function */
      return heap_alloc(heap, size, page_align);
    }

    /* update last header */
    struct mm_header_t *header = (struct mm_header_t *) heap->index.items[last_item_idx];
    header->size += heap->end_address - heap->start_address - old_length;

    /* update footer */
    struct mm_footer_t *footer = (struct mm_footer_t *) ((uint32_t) header + header->size - sizeof(struct mm_footer_t));
    footer->header = header;
    footer->magic = HEAP_MAGIC;

    /* we should have enough space now : recall the alloc function */
    return heap_alloc(heap, size, page_align);
  }

  /* get found hole informations */
  struct mm_header_t *orig_hole_header = (struct mm_header_t *) heap->index.items[i];
  uint32_t orig_hole_addr = (uint32_t) orig_hole_header;
  uint32_t orig_hole_size = orig_hole_header->size;

  /* check if the matching hole has to be split. if origin size is near requested size, give up. */
  if (orig_hole_size - new_size < sizeof(struct mm_header_t) + sizeof(struct mm_footer_t)) {
    size += orig_hole_size - new_size;
    new_size = orig_hole_size;
  }

  /* if we need to page align, make a new hole in front of our block */
  if (page_align && (orig_hole_addr & 0xFFFFF000) != 0) {
    /* go to next start page */
    uint32_t new_location = orig_hole_addr + PAGE_SIZE;

    /* create hole block until next page */
    struct mm_header_t *hole_header = (struct mm_header_t *) orig_hole_addr;
    hole_header->size = PAGE_SIZE - (orig_hole_addr & 0xFFF) - sizeof(struct mm_header_t);
    hole_header->magic = HEAP_MAGIC;
    hole_header->is_hole = 1;

    struct mm_footer_t *hole_footer = (struct mm_footer_t *) ((uint32_t) new_location - sizeof(struct mm_footer_t));
    hole_footer->magic = HEAP_MAGIC;
    hole_footer->header = hole_header;

    /* update our matching hole */
    orig_hole_addr = new_location;
    orig_hole_size = orig_hole_size - hole_header->size;
  } else {
    /* remove the hole from the list */
    ordered_array_remove(&heap->index, i);
  }

  /* create block header */
  struct mm_header_t *block_header = (struct mm_header_t *) orig_hole_addr;
  block_header->magic = HEAP_MAGIC;
  block_header->is_hole = 0;
  block_header->size = new_size;

  /* create block footer */
  struct mm_footer_t *block_footer = (struct mm_footer_t *) (orig_hole_addr + sizeof(struct mm_header_t) + size);
  block_footer->magic = HEAP_MAGIC;
  block_footer->header = block_header;

  /* write remaining hole after this block */
  if (orig_hole_size - new_size > 0) {
    struct mm_header_t *hole_header = (struct mm_header_t *) (orig_hole_addr + sizeof(struct mm_header_t) + size + sizeof(struct mm_footer_t));
    hole_header->magic = HEAP_MAGIC;
    hole_header->is_hole = 1;
    hole_header->size = orig_hole_size - new_size;

    struct mm_footer_t *hole_footer = (struct mm_footer_t *) ((uint32_t) hole_header + orig_hole_size - new_size - sizeof(struct mm_footer_t));
    if ((uint32_t) hole_footer < heap->end_address) {
      hole_footer->magic = HEAP_MAGIC;
      hole_footer->header = hole_header;
    }

    /* index new hole */
    ordered_array_insert(&heap->index, (void *) hole_header);
  }

  return (void *) ((uint32_t) block_header + sizeof(struct mm_header_t));
}

/*
 * Free memory from a heap.
 */
void heap_free(struct heap_t *heap, void *p)
{
  struct mm_header_t *header;
  struct mm_footer_t *footer;
  uint32_t cache_size, i;
  char do_add = 1;

  /* don't free null pointers */
  if (p == 0)
    return;

  /* get header and footer */
  header = (struct mm_header_t *) ((uint32_t) p - sizeof(struct mm_header_t));
  footer = (struct mm_footer_t *) ((uint32_t) header + header->size - sizeof(struct mm_footer_t));

  /* mark this block as a hole */
  header->is_hole = 1;

  /* merge with left hole if needed */
  struct mm_footer_t *footer_left = (struct mm_footer_t *) ((uint32_t) header - sizeof(struct mm_footer_t));
  if (footer_left->magic == HEAP_MAGIC && footer_left->header->is_hole == 1) {
    cache_size = header->size;
    header = footer_left->header;
    footer->header = header;
    header->size += cache_size;
    do_add = 0;
  }

  /* merge with right hole if needed */
  struct mm_header_t *header_right = (struct mm_header_t *) ((uint32_t) footer + sizeof(struct mm_footer_t));
  if (header_right->magic == HEAP_MAGIC && header_right->is_hole == 1) {
    header->size += header_right->size;
    footer = (struct mm_footer_t *) ((uint32_t) header_right + header_right->size - sizeof(struct mm_footer_t));
    footer->header = header;

    /* find right hole index */
    for (i = 0; i < heap->index.size; i++)
      if (heap->index.items[i] == header_right)
        break;

    /* remove hole from index */
    if (i < heap->index.size)
      ordered_array_remove(&heap->index, i);
  }

  /* add free block if needed */
  if (do_add)
    ordered_array_insert(&heap->index, (void *) header);
}
