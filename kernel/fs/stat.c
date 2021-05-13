#include <fs/fs.h>
#include <mm/mm.h>
#include <stderr.h>

/*
 * Stat system call.
 */
int do_stat(const char *filename, struct stat_t *statbuf)
{
  struct inode_t *inode;

  /* get inode */
  inode = namei(filename);
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

  /* get inode */
  //inode = namei(pathname);
  //if (!inode)
  //  return -ENOENT;

  printf("%d %s\n", dirfd, pathname);

  return 0;
}

/*
 * Check access file.
 */
int do_access(const char *filename)
{
  struct inode_t *inode;

  /* get inode */
  inode = namei(filename);
  if (!inode)
    return -ENOENT;

  return 0;
}
