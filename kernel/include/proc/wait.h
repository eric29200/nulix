#ifndef _WAIT_H_
#define _WAIT_H_

#include <stddef.h>

#define WNOHANG				1
#define WUNTRACED			2
#define WSTOPPED			2
#define WEXITED				4
#define WCONTINUED			8
#define WNOWAIT				0x1000000

#define MAX_POLL_TABLE_ENTRIES		((PAGE_SIZE - sizeof(struct poll_table)) / sizeof(struct poll_table_entry))

#define WAIT_QUEUE_HEAD(x) 		((struct wait_queue *)((x) - 1))

/*
 * Wait queue.
 */
struct wait_queue {
	struct task *			task;		/* task in wait queue */
	struct wait_queue *		next;		/* next task in wait queue */
};

struct poll_table_entry {
	struct wait_queue		wait;
	struct wait_queue **		wait_address;
};

struct poll_table {
	size_t				nr;
	struct poll_table_entry *	entry;
};

#endif
