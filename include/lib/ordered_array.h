#ifndef _ORDERED_ARRAY_H_
#define _ORDERED_ARRAY_H_

#include <lib/stddef.h>

struct ordered_array_t {
  void **items;
  uint32_t size;
  uint32_t max_size;
  int8_t (*comparator)(void *, void *);
};

struct ordered_array_t ordered_array_create(void *addr, uint32_t max_size, int8_t (*comparator)(void *, void *));
void ordered_array_insert(struct ordered_array_t *array, void *item);
void ordered_array_remove(struct ordered_array_t *array, uint32_t i);

#endif
