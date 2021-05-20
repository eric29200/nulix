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
  inode = namei(dirfd, filename);
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

  /* unused mask and flags */
  UNUSED(flags);
  UNUSED(mask);

  /* get inode */
  inode = namei(dirfd, pathname);
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
  struct buffer_head_t *bh;
  struct inode_t *inode;
  size_t len;

  /* limit buffer size to block size */
  if (bufsize > BLOCK_SIZE - 1)
    bufsize = BLOCK_SIZE - 1;

  /* get inode */
  inode = namei(dirfd, pathname);
  if (!inode)
    return -ENOENT;

  /* check 1st block */
  if (!inode->i_zone[0]) {
    iput(inode);
    return 0;
  }

  /* read 1st block */
  bh = bread(inode->i_dev, inode->i_zone[0]);
  if (!bh) {
    iput(inode);
    return 0;
  }

  /* release inode */
  iput(inode);

  /* copy target name to user buffer */
  for (len = 0; len < bufsize; len++)
    buf[len] = bh->b_data[len];

  /* release buffer */
  brelse(bh);
  return len;
}
