#ifndef _WAIT_H_
#define _WAIT_H_

#include <stddef.h>

#define WNOHANG				1
#define WUNTRACED			2
#define WSTOPPED			2
#define WEXITED				4
#define WCONTINUED			8
#define WNOWAIT				0x1000000

#define MAX_SELECT_TABLE_ENTRIES	((PAGE_SIZE) / sizeof(struct select_table_entry_t))

/*
 * Wait queue.
 */
struct wait_queue_t {
	struct task_t *			task;		/* task in wait queue */
	struct wait_queue_t *		next;		/* next task in wait queue */
};

struct select_table_entry_t {
	struct wait_queue_t		wait;
	struct wait_queue_t **		wait_address;
};

struct select_table_t {
	size_t				nr;
	struct select_table_entry_t *	entry;
};

#endif
