#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>

/* global file table (defined in open.c) */
extern struct file_t filp_table[NR_FILE];

/*
 * Read from a pipe.
 */
int read_pipe(struct inode_t *inode, char *buf, int count)
{
  int chars, size, rpos, read = 0;

  while (count > 0) {
    /* no data available */
    while (!(size = PIPE_SIZE(inode))) {
      /* wake up writer */
      task_wakeup(&inode->i_wwait);

      /* no writer : return */
      if (inode->i_ref != 2)
        return read;

      /* wait for some data */
      task_sleep(&inode->i_rwait);
    }

    /* compute number of characters to read */
    chars = PAGE_SIZE - PIPE_RPOS(inode);
    if (chars > count)
      chars = count;
    if (chars > size)
      chars = size;

    /* update size */
    count -= chars;
    read += chars;

    /* update pipe read position */
    rpos = PIPE_RPOS(inode);
    PIPE_RPOS(inode) += chars;
    PIPE_RPOS(inode) &= (PAGE_SIZE - 1);

    /* copy data to buffer */
    memcpy(buf, &((char *) inode->i_size)[rpos], chars);
    buf += chars;
  }

  /* wake up writer */
  task_wakeup(&inode->i_wwait);
  return read;
}

/*
 * Write to a pipe.
 */
int write_pipe(struct inode_t *inode, const char *buf, int count)
{
  int chars, size, wpos, written = 0;

  while (count > 0) {
    /* no free space */
    while (!(size = (PAGE_SIZE - 1) - PIPE_SIZE(inode))) {
      /* wake up reader */
      task_wakeup(&inode->i_rwait);

      /* no reader : return */
      if (inode->i_ref != 2)
        return written ? written : -ENOSPC;

      /* wait for free space */
      task_sleep(&inode->i_wwait);
    }

    /* compute number of characters to write */
    chars = PAGE_SIZE - PIPE_WPOS(inode);
    if (chars > count)
      chars = count;
    if (chars > size)
      chars = size;

    /* update size */
    count -= chars;
    written += chars;

    /* update pipe write position */
    wpos = PIPE_WPOS(inode);
    PIPE_WPOS(inode) += chars;
    PIPE_WPOS(inode) &= (PAGE_SIZE - 1);

    /* copy data to memory */
    memcpy(&((char *) inode->i_size)[wpos], buf, chars);
    buf += chars;
  }

  /* wake up reader */
  task_wakeup(&inode->i_rwait);
  return written;
}

/*
 * Pipe system call.
 */
int do_pipe(int pipefd[2])
{
  struct file_t *filps[2];
  struct inode_t *inode;
  int fd[2];
  int i, j;

  /* find 2 file descriptors in global table */
  for (i = 0, j = 0; i < NR_FILE && j < 2; i++)
    if (!filp_table[i].f_ref)
      filps[j++] = &filp_table[i];

  /* not enough available slots */
  if (j < 2)
    return -ENOSPC;

  /* update reference counts */
  filps[0]->f_ref++;
  filps[1]->f_ref++;

  /* find 2 file descriptors in current task */
  for (i = 0, j = 0; i < NR_OPEN && j < 2; i++) {
    if (!current_task->filp[i]) {
      fd[j] = i;
      current_task->filp[i] = filps[j++];
    }
  }

  /* not enough available slots */
  if (j < 2) {
    if (j == 1)
      current_task->filp[fd[0]] = NULL;

    filps[0]->f_ref = 0;
    filps[1]->f_ref = 0;
    return -ENOSPC;
  }

  /* get a pipe inode */
  inode = get_pipe_inode();
  if (!inode) {
    current_task->filp[fd[0]] = NULL;
    current_task->filp[fd[1]] = NULL;
    filps[0]->f_ref = 0;
    filps[1]->f_ref = 0;
    return -ENOSPC;
  }

  /* set 1st file descriptor as read channel */
  filps[0]->f_inode = inode;
  filps[0]->f_pos = 0;
  filps[0]->f_mode = 1;
  pipefd[0] = fd[0];

  /* set 2nd file descriptor as write channel */
  filps[1]->f_inode = inode;
  filps[1]->f_pos = 0;
  filps[1]->f_mode = 2;
  pipefd[1] = fd[1];

  return 0;
}
