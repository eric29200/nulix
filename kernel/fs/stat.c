#include <fs/fs.h>
#include <mm/mm.h>
#include <proc/sched.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

/*
 * Stat system call.
 */
int do_stat(const char *filename, struct stat_t *statbuf)
{
  struct inode_t *inode;

  /* get inode */
  inode = namei(filename, NULL);
  if (!inode)
    return -ENOENT;

  /* copy stats */
  statbuf->st_ino = inode->i_ino;
  statbuf->st_mode = inode->i_mode;
  statbuf->st_nlinks = inode->i_nlinks;
  statbuf->st_uid = inode->i_uid;
  statbuf->st_gid = inode->i_gid;
  statbuf->st_size = inode->i_size;
  statbuf->st_atime = inode->i_time;
  statbuf->st_mtime = inode->i_time;
  statbuf->st_ctime = inode->i_time;

  /* release inode */
  iput(inode);

  return 0;
}

/*
 * Statx system call.
 */
int do_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx_t *statbuf)
{
  struct inode_t *inode;
  struct file_t *filp;

  /* unused mask and flags */
  UNUSED(flags);
  UNUSED(mask);

  /* get dir fd file */
  if (dirfd >= 0 && dirfd < NR_OPEN)
    filp = current_task->filp[dirfd];

  /*
   * Prior (see man statx) :
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
    printf("statx (dirfd = %d, pathname = %s, flags = %x) not implemented\n", dirfd, pathname, flags);
    return -EINVAL;
  }

  /* check inode */
  if (!inode)
    return -ENOENT;

  /* reset stat buf */
  memset(statbuf, 0, sizeof(struct statx_t));

  /* fill stat */
  statbuf->stx_mask |= STATX_BASIC_STATS;
  statbuf->stx_blksize = BLOCK_SIZE;
  statbuf->stx_nlink = inode->i_nlinks;
  statbuf->stx_uid = inode->i_uid;
  statbuf->stx_gid = inode->i_gid;
  statbuf->stx_mode = inode->i_mode;
  statbuf->stx_ino = inode->i_ino;
  statbuf->stx_size = inode->i_size;
  statbuf->stx_atime.tv_sec = inode->i_time;
  statbuf->stx_btime.tv_sec = inode->i_time;
  statbuf->stx_ctime.tv_sec = inode->i_time;
  statbuf->stx_mtime.tv_sec = inode->i_time;

  /* release inode */
  iput(inode);
  return 0;
}

/*
 * Check access file.
 */
int do_access(const char *filename, mode_t mode)
{
  struct inode_t *inode;

  UNUSED(mode);

  /* get inode */
  inode = namei(filename, NULL);
  if (!inode)
    return -ENOENT;

  /* release inode */
  iput(inode);
  return 0;
}

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
