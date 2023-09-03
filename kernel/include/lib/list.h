#ifndef _LIST_H_
#define _LIST_H_

/*
 * List structure.
 */
struct list_head {
	struct list_head *prev;
	struct list_head *next;
};

#define LIST_HEAD_INIT(name)				{ &(name), &(name) }
#define LIST_HEAD(name)					struct list_head name = LIST_HEAD_INIT(name)

#define list_entry(ptr, type, member)			container_of(ptr, type, member)
#define list_first_entry(ptr, type, member)		list_entry((ptr)->next, type, member)
#define list_next_entry(pos, member)			list_entry((pos)->member.next, typeof(*(pos)), member)
#define list_next_entry_or_null(pos, head, member)	(list_is_last(&(pos)->member, head) ? NULL : list_next_entry(pos, member))

#define list_for_each(pos, head)			for (pos = (head)->next; pos != (head); pos = pos->next)
#define list_for_each_safe(pos, n, head)		for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

/*
 * Init a list head structure.
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

/*
 * Insert a new entry between prev and next.
 */
static inline void __list_add(struct list_head *new, struct list_head *prev, struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/*
 * Insert a new entry after head.
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/*
 * Insert a new entry before head.
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

/*
 * Delete entries between prev and next.
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	prev->next = next;
	next->prev = prev;
}

/*
 * Delete an entry.
 */
static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->prev = (void *) 0;
	entry->next = (void *) 0;
}

/*
 * Tests if a list is empty.
 */
static inline int list_empty(struct list_head *head)
{
	return head->next == head;
}

/*
 * Tests if list is last element.
 */
static inline int list_is_last(const struct list_head *list, const struct list_head *head)
{
	return list->next == head;
}

#endif
