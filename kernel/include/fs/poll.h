#ifndef _POLL_H_
#define _POLL_H_

#include <stddef.h>
#include <proc/wait.h>

#define POLLIN		0x0001
#define POLLPRI		0x0002
#define POLLOUT		0x0004
#define POLLERR		0x0008
#define POLLHUP		0x0010
#define POLLNVAL	0x0020
#define POLLRDNORM	0x0040
#define POLLRDBAND	0x0080
#define POLLWRNORM	0x0100
#define POLLWRBAND	0x0200
#define POLLMSG		0x0400

#define MAX_POLL_TABLE_ENTRIES			((PAGE_SIZE) / sizeof(struct poll_table_entry))

/*
 * Poll callback function.
 */
struct poll_table;
typedef void (*poll_queue_proc)(struct wait_queue_head *, struct poll_table *);

/*
 * Poll file descriptor.
 */
struct pollfd {
	int		fd;
	uint16_t	events;
	uint16_t	revents;
};

/*
 * Poll table entry.
 */
struct poll_table_entry {
	struct wait_queue		wait;
	struct wait_queue_head *	wait_address;
};

/*
 * Poll table.
 */
struct poll_table {
	poll_queue_proc			qproc;
	size_t				nr;
	struct poll_table_entry *	entry;
};

/*
 * Set poll callback function.
 */
static inline void init_poll_funcptr(struct poll_table *pt, poll_queue_proc qproc)
{
	pt->qproc = qproc;
}

/*
 * Add a wait queue to a poll table.
 */
static inline void poll_wait(struct wait_queue_head *wait_address, struct poll_table *pt)
{
	if (pt && wait_address)
		pt->qproc(wait_address, pt);
}

#endif
