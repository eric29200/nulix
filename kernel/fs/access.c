#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Faccessat system call.
 */
int do_faccessat(int dirfd, const char *pathname, int flags)
{
  struct inode_t *inode;

  /* unused flags */
  UNUSED(flags);

  /* check inode */
  inode = namei(dirfd, pathname, 1);
  if (!inode)
    return -ENOENT;

  /* release inode */
  iput(inode);
  return 0;
}
