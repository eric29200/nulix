#include <fs/fs.h>
#include <proc/sched.h>
#include <ipc/signal.h>
#include <mm/mm.h>
#include <stderr.h>
#include <time.h>

/*
 * Free a select table.
 */
static void free_wait(struct select_table_t *st)
{
	struct select_table_entry_t *entry = st->entry + st->nr;

	while (st->nr > 0) {
		st->nr--;
		entry--;
		remove_wait_queue(entry->wait_address, &entry->wait);
	}
}

/*
 * Poll a list of file descriptors.
 */
static void do_pollfd(struct pollfd_t *fds, int *count, struct select_table_t *wait)
{
	struct file_t *filp;
	uint32_t mask = 0;
	int fd;

	/* reset mask and number of events */
	mask = 0;
	*count = 0;

	/* check file descriptor */
	fd = fds->fd;
	if (fd >= 0 && fd < NR_OPEN && current_task->files->filp[fd]) {
		filp = current_task->files->filp[fd];

		/* call specific poll */
		mask = POLLNVAL;
		if (filp->f_op && filp->f_op->poll)
			mask = filp->f_op->poll(filp, wait);

		/* update mask */
		mask &= fds->events | POLLERR | POLLHUP;
	}

	/* set number of events */
	if (mask)
		*count += 1;

	/* set output events */
	fds->revents = mask;
}

/*
 * Poll system call.
 */
int do_poll(struct pollfd_t *fds, size_t ndfs, int timeout)
{
	struct select_table_t wait_table, *wait;
	struct select_table_entry_t *entry;
	int count = 0;
	size_t i;

	/* allocate table entry */
	entry = (struct select_table_entry_t *) get_free_page();
	if (!entry)
		return -ENOMEM;

	/* set wait table */
	wait_table.nr = 0;
	wait_table.entry = entry;
	wait = &wait_table;

	/* set time out */
	if (timeout > 0)
		current_task->timeout = jiffies + ms_to_jiffies(timeout);
	else
		current_task->timeout = 0;

	for (;;) {
		/* poll each file */
		for (i = 0; i < ndfs; i++)
			do_pollfd(&fds[i], &count, wait);
		wait = NULL;

		/* events catched or signal transmitted : break */
		if (count || !sigisemptyset(&current_task->sigpend) || !timeout || jiffies >= current_task->timeout)
			break;

		/* no events sleep */
		current_task->state = TASK_SLEEPING;
		schedule();
	}

	/* reset timeout */
	current_task->timeout = 0;

	/* free wait table */
	free_wait(&wait_table);
	free_page(entry);

	return count;
}

/*
 * Check events on a file.
 */
static int __select_check(int fd, uint16_t mask, struct select_table_t *wait)
{
	struct pollfd_t pollfd;
	int count;

	/* set poll structure */
	pollfd.fd = fd;
	pollfd.events = mask;
	pollfd.revents = 0;

	/* poll file */
	do_pollfd(&pollfd, &count, wait);

	return count;
}

/*
 * Select system call.
 */
int do_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct kernel_timeval_t *timeout)
{
	fd_set_t res_readfds, res_writefds, res_exceptfds;
	struct select_table_t wait_table, *wait;
	struct select_table_entry_t *entry;
	int i, max = -1, count;
	uint32_t set, j;

	/* adjust number of file descriptors */
	if (nfds < 0)
		return -EINVAL;
	if (nfds > NR_OPEN)
		nfds = NR_OPEN;

	/* check file descriptors */
	for (j = 0; j < FDSET_INTS; j++) {
		i = j * NFD_BITS;
		if (i >= nfds)
			break;

		set = 0;
		if (readfds)
			set |= readfds->fds_bits[j];
		if (writefds)
			set |= writefds->fds_bits[j];
		if (exceptfds)
			set |= exceptfds->fds_bits[j];

		for (; set; i++, set >>= 1) {
			if (i >= nfds)
				goto end_check;

			if (!(set & 1))
				continue;

			if (!current_task->files->filp[i] || !current_task->files->filp[i]->f_inode)
				return -EBADF;

			max = i;
		}
	}

end_check:
	nfds = max + 1;
	count = 0;

	/* allocate table entry */
	entry = (struct select_table_entry_t *) get_free_page();
	if (!entry)
		return -ENOMEM;

	/* set wait table */
	wait_table.nr = 0;
	wait_table.entry = entry;
	wait = &wait_table;

	/* zero results */
	memset(&res_readfds, 0, sizeof(fd_set_t));
	memset(&res_writefds, 0, sizeof(fd_set_t));
	memset(&res_exceptfds, 0, sizeof(fd_set_t));

	/* set time out */
	if (timeout && (timeout->tv_sec > 0 || timeout->tv_nsec > 0))
		current_task->timeout = jiffies + kernel_timeval_to_jiffies(timeout);
	else
		current_task->timeout = 0;

	/* loop until events occured */
	for (;;) {
		for (i = 0; i < nfds; i++) {
			/* check events */
			if (readfds && FD_ISSET(i, readfds) && __select_check(i, POLLIN, wait)) {
				FD_SET(i, &res_readfds);
				count++;
			}

			if (writefds && FD_ISSET(i, writefds) && __select_check(i, POLLOUT, wait)) {
				FD_SET(i, &res_writefds);
				count++;
			}

			if (exceptfds && FD_ISSET(i, exceptfds) && __select_check(i, POLLPRI, wait)) {
				FD_SET(i, &res_exceptfds);
				count++;
			}
		}
		wait = NULL;

		/* events catched : break */
		if (count)
			break;

		/* signal catched : break */
		if (!sigisemptyset(&current_task->sigpend))
			break;

		/* timeout : break */
		if ((timeout->tv_sec == 0 && timeout->tv_nsec == 0) || (timeout && jiffies >= current_task->timeout))
			break;

		/* no events sleep */
		current_task->state = TASK_SLEEPING;
		schedule();
	}

	/* reset timeout */
	current_task->timeout = 0;

	/* copy results */
	if (readfds)
		memcpy(readfds, &res_readfds, sizeof(fd_set_t));
	if (writefds)
		memcpy(writefds, &res_writefds, sizeof(fd_set_t));
	if (exceptfds)
		memcpy(exceptfds, &res_exceptfds, sizeof(fd_set_t));

	/* free wait table */
	free_wait(&wait_table);
	free_page(entry);

	return count;
}
