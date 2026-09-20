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

/*
 * Wait queue callback.
 */
struct wait_queue;
typedef int (*wait_queue_func_t)(struct wait_queue *);

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
	wait_queue_func_t		func;
	struct list_head		task_list;
};

extern int default_wake_function(struct wait_queue *wait);

#define WAITQUEUE_INITIALIZER(tsk) 		{ .task = tsk, .func = default_wake_function, .task_list = { NULL, NULL } }
#define DECLARE_WAITQUEUE(name, tsk)		struct wait_queue name = WAITQUEUE_INITIALIZER(tsk)

#define WAIT_QUEUE_HEAD_INITIALIZER(name)	{ .task_list = { &(name).task_list, &(name).task_list }, }
#define DECLARE_WAIT_QUEUE_HEAD(name)		struct wait_queue_head name = WAIT_QUEUE_HEAD_INITIALIZER(name)

#endif
