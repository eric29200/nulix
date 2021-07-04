#include <fs/fs.h>
#include <proc/sched.h>
#include <ipc/signal.h>
#include <time.h>

/*
 * Poll a list of file descriptors.
 */
static void do_pollfd(struct pollfd_t *fds, int *count)
{
  struct file_t *filp;
  uint32_t mask = 0;
  int fd;

  /* for each file descriptor */
  mask = 0;

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
      task_sleep_timeout(current_task->waiting_chan, timeout);
      timeout = 0;
    } else if (timeout == 0) {
      return count;
    } else {
      task_sleep(current_task->waiting_chan);
    }
  }

  return count;
}

/*
 * Select system call.
 */
int do_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timeval_t *timeout)
{
  printf("*****************************");
  return 0;
}
