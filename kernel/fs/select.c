#include <fs/fs.h>
#include <proc/sched.h>
#include <ipc/signal.h>
#include <stderr.h>
#include <time.h>

/*
 * Poll a list of file descriptors.
 */
static void do_pollfd(struct pollfd_t *fds, int *count)
{
	struct file_t *filp;
	uint32_t mask = 0;
	int fd;

	/* reset mask and number of events */
	mask = 0;
	*count = 0;

	/* check file descriptor */
	fd = fds->fd;
	if (fd >= 0 && fd < NR_FILE && current_task->filp[fd]) {
		filp = current_task->filp[fd];

		/* call specific poll */
		mask = POLLNVAL;
		if (filp->f_op && filp->f_op->poll)
			mask = filp->f_op->poll(filp);

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
	int count = 0;
	size_t i;

	for (;;) {
		/* poll each file */
		for (i = 0; i < ndfs; i++)
			do_pollfd(&fds[i], &count);

		/* events catched or signal transmitted : break */
		if (count || !sigisemptyset(&current_task->sigpend) || !timeout)
			break;

		/* no events : sleep */
		if (timeout > 0) {
			task_sleep_timeout_ms(current_task->waiting_chan, timeout);
			timeout = -1;
		} else if (timeout == 0) {
			return count;
		} else {
			task_sleep(current_task->waiting_chan);
		}
	}

	return count;
}

/*
 * Check events on a file.
 */
static int __select_check(int fd, uint16_t mask)
{
	struct pollfd_t pollfd;
	int count;

	/* set poll structure */
	pollfd.fd = fd;
	pollfd.events = mask;
	pollfd.revents = 0;

	/* poll file */
	do_pollfd(&pollfd, &count);

	return count;
}

/*
 * Select system call.
 */
int do_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timespec_t *timeout)
{
	fd_set_t res_readfds, res_writefds, res_exceptfds;
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

		set = readfds->fds_bits[j] | writefds->fds_bits[j] | exceptfds->fds_bits[j];
		for (; set; i++, set >>= 1) {
			if (i >= nfds)
				goto end_check;

			if (!(set & 1))
				continue;

			if (!current_task->filp[i] || !current_task->filp[i]->f_inode)
				return -EBADF;

			max = i;
		}
	}

end_check:
	nfds = max + 1;
	count = 0;

	/* zero results */
	memset(&res_readfds, 0, sizeof(fd_set_t));
	memset(&res_writefds, 0, sizeof(fd_set_t));
	memset(&res_exceptfds, 0, sizeof(fd_set_t));

	/* loop until events occured */
	for (;;) {
		for (i = 0; i < nfds; i++) {
			/* check events */
			if (FD_ISSET(i, readfds) && __select_check(i, POLLIN)) {
				FD_SET(i, &res_readfds);
				count++;
			}

			if (FD_ISSET(i, writefds) && __select_check(i, POLLOUT)) {
				FD_SET(i, &res_writefds);
				count++;
			}

			if (FD_ISSET(i, exceptfds) && __select_check(i, POLLIN | POLLOUT)) {
				FD_SET(i, &res_exceptfds);
				count++;
			}
		}

		/* events catched or signal transmitted : break */
		if (count || !sigisemptyset(&current_task->sigpend))
			break;

		/* no events : sleep */
		if (timeout == NULL) {
			task_sleep(current_task->waiting_chan);
		} else if (timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
			return count;
		} else {
			task_sleep_timeout(current_task->waiting_chan, timeout);
			timeout = NULL;
		}
	}

	/* copy results */
	memcpy(readfds, &res_readfds, sizeof(fd_set_t));
	memcpy(writefds, &res_writefds, sizeof(fd_set_t));
	memcpy(exceptfds, &res_exceptfds, sizeof(fd_set_t));

	return count;
}
