#include <fs/fs.h>
#include <proc/sched.h>
#include <ipc/signal.h>
#include <mm/mm.h>
#include <stderr.h>
#include <time.h>

/*
 * Free a select table.
 */
static void free_wait(struct select_table *st)
{
	struct select_table_entry *entry = st->entry + st->nr;

	while (st->nr > 0) {
		st->nr--;
		entry--;
		remove_wait_queue(&entry->wait);
	}
}

/*
 * Poll a list of file descriptors.
 */
static void do_pollfd(struct pollfd *fds, int *count, struct select_table *wait)
{
	struct file *filp;
	uint32_t mask = 0;

	/* reset mask and number of events */
	mask = 0;
	*count = 0;

	/* get file */
	filp = fget(fds->fd);
	if (filp) {
		/* call specific poll */
		mask = POLLNVAL;
		if (filp->f_op && filp->f_op->poll)
			mask = filp->f_op->poll(filp, wait);

		/* update mask */
		mask &= fds->events | POLLERR | POLLHUP;

		/* release file */
		fput(filp);
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
static int do_poll(struct pollfd *fds, size_t ndfs, int timeout)
{
	struct select_table wait_table, *wait;
	struct select_table_entry *entry;
	int count = 0;
	size_t i;

	/* allocate table entry */
	entry = (struct select_table_entry *) get_free_page();
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

		/* signal interruption */
		if (!count && signal_pending(current_task))
			count = -EINTR;

		/* events catched or timeout : break */
		if (count || !timeout || (timeout > 0 && jiffies >= current_task->timeout))
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
 * Poll system call.
 */
int sys_poll(struct pollfd *fds, size_t nfds, int timeout)
{
	return do_poll(fds, nfds, timeout);
}

/*
 * Check events on a file.
 */
static int __select_check(int fd, uint16_t mask, struct select_table *wait)
{
	struct pollfd pollfd;
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
static int do_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct kernel_timeval *timeout)
{
	fd_set_t res_readfds, res_writefds, res_exceptfds;
	struct select_table wait_table, *wait;
	struct select_table_entry *entry;
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

			if (!current_task->files->filp[i]
				|| !current_task->files->filp[i]->f_dentry
				|| !current_task->files->filp[i]->f_dentry->d_inode)
				return -EBADF;

			max = i;
		}
	}

end_check:
	nfds = max + 1;
	count = 0;

	/* allocate table entry */
	entry = (struct select_table_entry *) get_free_page();
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
		if (signal_pending(current_task))
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

	/* signal interruption */
	if (!count && signal_pending(current_task))
		return -EINTR;

	return count;
}

/*
 * Select system call.
 */
int sys_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct old_timeval *timeout)
{
	struct kernel_timeval tv;

	if (timeout)
		old_timeval_to_kernel_timeval(timeout, &tv);

	return do_select(nfds, readfds, writefds, exceptfds, timeout ? &tv : NULL);
}

/*
 * Pselect6 system call.
 */
int sys_pselect6(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct old_timeval *timeout, sigset_t *sigmask)
{
	struct kernel_timeval tv;
	sigset_t current_sigmask;
	int ret;

	/* handle sigmask */
	if (sigmask) {
		/* save current sigmask */
		current_sigmask = current_task->sigmask;

		/* set new sigmask (do not mask SIGKILL and SIGSTOP) */
		current_task->sigmask = *sigmask;
		sigdelset(&current_task->sigmask, SIGKILL);
		sigdelset(&current_task->sigmask, SIGSTOP);
	}

	/* convert timespec to kernel timeval */
	if (timeout)
		old_timeval_to_kernel_timeval(timeout, &tv);

	/* select */
	ret = do_select(nfds, readfds, writefds, exceptfds, timeout ? &tv : NULL);

	/* restore sigmask and delete masked pending signals */
	if (ret == -EINTR && sigmask)
		current_task->saved_sigmask = current_sigmask;
	else if (sigmask)
		current_task->sigmask = current_sigmask;

	return ret;
}
