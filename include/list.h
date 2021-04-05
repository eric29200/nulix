#ifndef _LIST_H_
#define _LIST_H_

#include <stddef.h>

/*
 * Circularly doubly linked list.
 */
struct list_t {
  struct list_t *prev;
  struct list_t *next;
};

#define list_entry(ptr, type, member)         container_of(ptr, type, member)
#define list_for_each(pos, head)              for (pos = (head)->next; pos != (head); pos = pos->next)

/*
 * Init a list.
 */
static inline void list_init(struct list_t *list)
{
  list->next = list;
  list->prev = list;
}

/*
 * Add a new entry between prev and next.
 */
static inline void __list_add(struct list_t *new, struct list_t *prev, struct list_t *next)
{
  next->prev = new;
  new->prev = prev;
  new->next = next;
  prev->next = new;
}

/*
 * Insert a new entry after head.
 */
static inline void list_add(struct list_t *new, struct list_t *head)
{
  __list_add(new, head, head->next);
}

/*
 * Insert a new entry before head.
 */
static inline void list_add_tail(struct list_t *new, struct list_t *head)
{
  __list_add(new, head->prev, head);
}

/*
 * Delete the elements between prev and next.
 */
static inline void __list_del(struct list_t *prev, struct list_t *next)
{
  prev->next = next;
  next->prev = prev;
}

/*
 * Delete entry from the list.
 */
static inline void list_del(struct list_t *entry)
{
  __list_del(entry->prev, entry->next);
}

/*
 * Tests if a list is empty.
 */
static inline int list_empty(const struct list_t *head)
{
  return head->next == head;
}

#endif
