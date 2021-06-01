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
int do_stat(int dirfd, const char *filename, struct stat_t *statbuf)
{
  struct inode_t *inode;

  /* get inode */
  inode = namei(dirfd, filename, 0);
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

  /* unused mask */
  UNUSED(mask);

  /* get inode */
  inode = namei(dirfd, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
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
 * Read value of a symbolic link.
 */
ssize_t do_readlink(int dirfd, const char *pathname, char *buf, size_t bufsize)
{
  struct inode_t *inode;

  /* get inode */
  inode = namei(dirfd, pathname, 0);
  if (!inode)
    return -ENOENT;

  /* readlink not implemented */
  if (!inode->i_op || !inode->i_op->readlink) {
    iput(inode);
    return -EACCES;
  }

  /* read link */
  return inode->i_op->readlink(inode, buf, bufsize);
}
