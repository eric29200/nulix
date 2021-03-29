#include <lib/ordered_array.h>
#include <lib/string.h>
#include <kernel/mm.h>

/*
 * Create a new ordered array at specific address.
 */
struct ordered_array_t ordered_array_create(void *addr, uint32_t max_size, int8_t (*comparator)(void *, void *))
{
  struct ordered_array_t ret;

  ret.items = addr;
  memset(ret.items, 0, max_size * sizeof(void **));
  ret.size = 0;
  ret.max_size = max_size;
  ret.comparator = comparator;

  return ret;
}

/*
 * Insert an item.
 */
void ordered_array_insert(struct ordered_array_t *array, void *item)
{
  uint32_t i = 0, j;

  /* find index */
  while (i < array->size && array->comparator(array->items[i], item))
    i++;

  /* last position */
  if (i == array->size) {
    array->items[array->size] = item;
    goto out;
  }

  /* shift items on the right */
  for (j = array->size; j > i; j--)
    array->items[j] = array->items[j - 1];

  /* set item */
  array->items[i] = item;

out:
  array->size++;
}

/*
 * Remove an item.
 */
void ordered_array_remove(struct ordered_array_t *array, uint32_t i)
{
  /* shift items on the left */
  for (; i < array->size - 1; i++)
    array->items[i] = array->items[i + 1];

  array->size--;
}
