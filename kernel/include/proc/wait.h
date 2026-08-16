#ifndef _WAIT_H_
#define _WAIT_H_

#include <stddef.h>
#include <lib/list.h>

#define WNOHANG					1
#define WUNTRACED				2
#define WSTOPPED				2
#define WEXITED					4
#define WCONTINUED				8
#define WNOWAIT					0x1000000

#define MAX_POLL_TABLE_ENTRIES			((PAGE_SIZE) / sizeof(struct poll_table_entry))

#define WAIT_QUEUE_HEAD_INITIALIZER(name)	{ .task_list = { &(name).task_list, &(name).task_list }, }
#define DECLARE_WAIT_QUEUE_HEAD(name)		struct wait_queue_head name = WAIT_QUEUE_HEAD_INITIALIZER(name)

/*
 * Wait queue.
 */
struct wait_queue_head {
	struct list_head		task_list;
};

struct wait_queue {
	struct task *			task;
	struct list_head		task_list;
};

/*
 * Poll table.
 */
struct poll_table_entry {
	struct wait_queue		wait;
	struct wait_queue_head *	wait_address;
};

struct poll_table {
	size_t				nr;
	struct poll_table_entry *	entry;
};

#endif
