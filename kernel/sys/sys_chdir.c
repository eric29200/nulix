#include <sys/syscall.h>
#include <proc/sched.h>
#include <fs/fs.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Change directory system call.
 */
int sys_chdir(const char *path)
{
  struct inode_t *inode;

  /* get inode */
  inode = namei(path);
  if (!inode)
    return -ENOENT;

  /* check directory */
  if (!S_ISDIR(inode->i_mode)) {
    iput(inode);
    return -ENOTDIR;
  }

  /* release current working dir */
  iput(current_task->cwd);

  /* set current working dir */
  current_task->cwd = inode;

  return 0;
}
