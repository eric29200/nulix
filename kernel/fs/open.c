#include <fs/fs.h>
#include <fs/buffer.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <stat.h>
#include <string.h>

/*
 * Open system call.
 */
int sys_open(const char *pathname)
{
  struct inode_t *inode;
  struct file_t *file;
  int fd, ret;

  /* find a free slot in current process */
  for (fd = 0; fd < NR_OPEN; fd++)
    if (!current_task->filp[fd])
      break;

  /* no slots : exit */
  if (fd >= NR_OPEN)
    return -EINVAL;

  /* open file */
  ret = open_namei(pathname, &inode);
  if (ret != 0)
    return ret;

  /* allocate a new file */
  file = (struct file_t *) kmalloc(sizeof(struct file_t));
  if (!file) {
    kfree(inode);
    return -ENOMEM;
  }

  /* ttys (major = 4) */
  if (S_ISCHR(inode->i_mode) && major(inode->i_zone[0]) == 4)
    current_task->tty = inode->i_zone[0];

  /* set file */
  file->f_mode = inode->i_mode;
  file->f_inode = inode;
  file->f_pos = 0;
  current_task->filp[fd] = file;

  return fd;
}

/*
 * Close system call.
 */
int sys_close(int fd)
{
  struct file_t *filp;

  if (fd < 0 || fd >= NR_OPEN)
    return -EINVAL;

  filp = current_task->filp[fd];
  if (!filp)
    return -ENOMEM;

  current_task->filp[fd] = NULL;
  if (filp->f_inode)
    kfree(filp->f_inode);
  kfree(filp);

  return 0;
}
