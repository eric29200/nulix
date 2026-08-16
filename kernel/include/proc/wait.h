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

#define WAIT_QUEUE_HEAD_INITIALIZER(name)	{ .task_list = { &(name).task_list, &(name).task_list }, }
#define DECLARE_WAIT_QUEUE_HEAD(name)		struct wait_queue_head name = WAIT_QUEUE_HEAD_INITIALIZER(name)

/*
 * Wait queue head.
 */
struct wait_queue_head {
	struct list_head		task_list;
};

/*
 * Wait queue.
 */
struct wait_queue {
	struct task *			task;
	struct list_head		task_list;
};

#endif
