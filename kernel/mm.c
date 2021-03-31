#include <kernel/mm.h>
#include <kernel/mm_heap.h>
#include <lib/string.h>
#include <lib/stdio.h>

/* placement address (used before kernel heap is created) */
uint32_t placement_address = 0;
uint32_t placement_address_max = 0;

/* kernel heap */
struct heap_t *kheap = NULL;

/*
 * Allocate memory.
 */
void *kmalloc(uint32_t size)
{
  uint32_t ret;

  /* use kernel heap */
  if (kheap)
    return heap_alloc(kheap, size);

  /* check if memory is full */
  if (placement_address + size > placement_address_max)
    return NULL;

  /* else use placement address */
  ret = placement_address;
  placement_address += size;

  return (void *) ret;
}

/*
 * Free memory on the kernel heap.
 */
void kfree(void *p)
{
  heap_free( p);
}

/*
 * Init memory.
 */
void init_mem(uint32_t start, uint32_t end)
{
  placement_address = start;
  placement_address_max = end;

  /* create kernel heap */
  kheap = heap_create(placement_address + sizeof(struct heap_t), placement_address_max);
}
