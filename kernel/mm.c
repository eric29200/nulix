#include <kernel/mm.h>
#include <lib/string.h>
#include <lib/stdio.h>

/* placement address (used before kernel heap is created) */
uint32_t placement_address = 0;
uint32_t placement_address_max = 0;

/*
 * Allocate memory.
 */
void *kmalloc(uint32_t size)
{
  uint32_t ret = placement_address;
  placement_address += size;

  /* check if memory is full */
  if (placement_address > placement_address_max)
    panic("Kernel memory full");

  return (void *) ret;
}

/*
 * Init memory.
 */
void init_mem(uint32_t start, uint32_t end)
{
  placement_address = start;
  placement_address_max = end;
}
