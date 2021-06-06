#ifndef _HTABLE_H_
#define _HTABLE_H_

#include <stddef.h>
#include <string.h>

#define htable_entry(ptr, type, member)          container_of(ptr, type, member)

/*
 * Hash table struct.
 */
struct htable_link_t {
  struct htable_link_t *next;
  struct htable_link_t **pprev;
};

/*
 * Hash function.
 */
static inline uint32_t hash_32(uint32_t val, uint32_t bits)
{
  val = (val ^ 61) ^ (val >> 16);
  val = val + (val << 3);
  val = val ^ (val >> 4);
  val = val * 0x27d4eb2d;
  val = val ^ (val >> 15);
  return val >> (32 - bits);
}

/*
 * Init a hash table.
 */
static inline void htable_init(struct htable_link_t **htable, uint32_t bits)
{
  memset(htable, 0, sizeof(struct htable_link_t *) * (1 << bits));
}

/*
 * Get an element from a hash table.
 */
static inline struct htable_link_t *htable_lookup(struct htable_link_t **htable, uint32_t key, uint32_t bits)
{
  return htable[hash_32(key, bits)];
}

/*
 * Insert an element into a hash table.
 */
static inline void htable_insert(struct htable_link_t **htable, struct htable_link_t *node, uint32_t key, uint32_t bits)
{
  int i;

  i = hash_32(key, bits);
  node->next = htable[i];
  node->pprev = &htable[i];
  if (htable[i])
    htable[i]->pprev = (struct htable_link_t **) node;
  htable[i] = node;
}

/*
 * Delete an element from a hash table.
 */
static inline void htable_delete(struct htable_link_t *node)
{
  struct htable_link_t *next = node->next;
  struct htable_link_t **pprev = node->pprev;

  *pprev = next;
  if (next)
    next->pprev = pprev;
}

#endif
