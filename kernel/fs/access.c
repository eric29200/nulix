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
  struct file_t *filp;

  /* unused flags */
  UNUSED(flags);

  /* get dir fd file */
  if (dirfd >= 0 && dirfd < NR_OPEN)
    filp = current_task->filp[dirfd];

  /*
   * Prior (see man faccessat) :
   * 1 - absolute path
   * 2 - relative path from cwd
   * 3 - directory (dirfd) relative path
   * 4 - file descriptor dirfd
   */
  if (pathname && (*pathname == '/' || dirfd == AT_FDCWD)) {
    inode = namei(pathname, NULL);
  } else if (filp && S_ISDIR(filp->f_inode->i_mode)) {
    inode = namei(pathname, filp->f_inode);
  } else if (dirfd == AT_EMPTY_PATH && filp) {
    inode = filp->f_inode;
  } else {
    printf("faccessat (dirfd = %d, pathname = %s, flags = %x) not implemented\n", dirfd, pathname, flags);
    return -EINVAL;
  }

  /* check inode */
  if (!inode)
    return -ENOENT;

  /* release inode */
  iput(inode);
  return 0;
}
