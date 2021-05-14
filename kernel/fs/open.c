#include <fs/fs.h>
#include <mm/mm.h>
#include <drivers/tty.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/* global file table */
static struct file_t filp_table[NR_FILE];

/*
 * Open system call.
 */
int do_open(const char *pathname, int flags, mode_t mode)
{
  struct inode_t *inode;
  int fd, i, ret;

  /* get a new tty */
  if (strcmp(pathname, "/dev/tty") == 0) {
    current_task->tty = tty_get();
    if ((int) current_task->tty < 0)
      return -EBUSY;
  }

  /* find a free slot in current process */
  for (fd = 0; fd < NR_OPEN; fd++)
    if (!current_task->filp[fd])
      break;

  /* no slots : exit */
  if (fd >= NR_OPEN)
    return -EINVAL;

  /* find a free file descriptor in global table */
  for (i = 0; i < NR_FILE; i++)
    if (!filp_table[i].f_ref)
      break;

  /* no free file in global table */
  if (i >= NR_FILE)
    return -EINVAL;

  /* open file */
  ret = open_namei(pathname, flags, mode, &inode);
  if (ret != 0)
    return ret;

  /* set file */
  current_task->filp[fd] = &filp_table[i];
  current_task->filp[fd]->f_mode = inode->i_mode;
  current_task->filp[fd]->f_inode = inode;
  current_task->filp[fd]->f_flags = flags;
  current_task->filp[fd]->f_pos = 0;
  current_task->filp[fd]->f_ref++;

  return fd;
}

/*
 * Close system call.
 */
int do_close(int fd)
{
  if (fd < 0 || fd >= NR_OPEN || !current_task->filp[fd])
    return -EINVAL;


  /* release file if not used anymore */
  current_task->filp[fd]->f_ref--;
  if (current_task->filp[fd]->f_ref <= 0) {
    iput(current_task->filp[fd]->f_inode);
    memset(current_task->filp[fd], 0, sizeof(struct file_t));
  }

  current_task->filp[fd] = NULL;

  return 0;
}
