#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/*
 * Follow a link.
 */
static struct inode_t *follow_link(struct inode_t *inode)
{
  struct inode_t *res_inode;

  /* not implemented : return inode */
  if (!inode || !inode->i_op || !inode->i_op->follow_link)
    return inode;

  inode->i_op->follow_link(inode, &res_inode);
  return res_inode;
}

/*
 * Resolve a path name to the inode of the top most directory.
 */
static struct inode_t *dir_namei(int dirfd, const char *pathname, const char **basename, size_t *basename_len)
{
  struct inode_t *inode, *tmp;
  const char *name;
  size_t name_len;
  int err;

  /* absolute or relative path */
  if (*pathname == '/') {
    inode = current_task->root;
    pathname++;
  } else if (dirfd == AT_FDCWD) {
    inode = current_task->cwd;
  } else if (dirfd >= 0 && dirfd < NR_OPEN && current_task->filp[dirfd]) {
    inode = current_task->filp[dirfd]->f_inode;
  }

  if (!inode)
    return NULL;

  /* update reference count */
  inode->i_ref++;

  while (1) {
    /* check if inode is a directory */
    if (!S_ISDIR(inode->i_mode)) {
      iput(inode);
      return NULL;
    }

    /* compute next path name */
    name = pathname;
    for (name_len = 0; *pathname && *pathname++ != '/'; name_len++);

    /* end : return current inode */
    if (!*pathname)
      break;

    /* skip empty folder */
    if (!name_len)
      continue;

    /* lookup not implemented */
    inode->i_ref++;
    if (!inode->i_op || !inode->i_op->lookup) {
      iput(inode);
      return NULL;
    }

    /* lookup file */
    err = inode->i_op->lookup(inode, name, name_len, &tmp);
    if (err) {
      iput(inode);
      return NULL;
    }

    /* follow symbolic links */
    inode = follow_link(tmp);
    if (!inode)
      return NULL;
  }

  *basename = name;
  *basename_len = name_len;
  return inode;
}

/*
 * Resolve a path name to the matching inode.
 */
struct inode_t *namei(int dirfd, const char *pathname, int follow_links)
{
  struct inode_t *dir, *inode;
  const char *basename;
  size_t basename_len;
  int err;

  /* find directory */
  dir = dir_namei(dirfd, pathname, &basename, &basename_len);
  if (!dir)
    return NULL;

  /* special case : '/' */
  if (!basename_len)
    return dir;

  /* lookup not implemented */
  if (!dir->i_op || !dir->i_op->lookup) {
    iput(dir);
    return NULL;
  }

  /* lookup file */
  dir->i_ref++;
  err = dir->i_op->lookup(dir, basename, basename_len, &inode);
  if (err) {
    iput(dir);
    return NULL;
  }

  /* follow symbolic link */
  if (follow_links)
    inode = follow_link(inode);

  iput(dir);
  return inode;
}

/*
 * Resolve and open a path name.
 */
int open_namei(int dirfd, const char *pathname, int flags, mode_t mode, struct inode_t **res_inode)
{
  struct inode_t *dir, *inode;
  const char *basename;
  size_t basename_len;
  int err;

  *res_inode = NULL;

  /* find directory */
  dir = dir_namei(dirfd, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;

  /* open a directory */
  if (!basename_len) {
    /* do not allow to create/truncate directories here */
    if (!(flags & (O_ACCMODE | O_CREAT | O_TRUNC))) {
      *res_inode = dir;
      return 0;
    }

    iput(dir);
    return -EISDIR;
  }

  /* set mode (needed if new file is created) */
  mode &= ~current_task->umask & 0777;
  mode |= S_IFREG;

  /* lookup not implemented */
  if (!dir->i_op || !dir->i_op->lookup) {
    iput(dir);
    return -EPERM;
  }

  /* lookup inode */
  dir->i_ref++;
  err = dir->i_op->lookup(dir, basename, basename_len, &inode);
  if (err) {
    /* no such entry */
    if (!(flags & O_CREAT)) {
      iput(dir);
      return -ENOENT;
    }

    /* create not implemented */
    if (!dir->i_op || !dir->i_op->create) {
      iput(dir);
      return -EPERM;
    }

    /* create new inode */
    dir->i_ref++;
    err = dir->i_op->create(dir, basename, basename_len, mode, res_inode);

    /* release directory */
    iput(dir);
    return err;
  }

  /* follow symbolic link */
  *res_inode = follow_link(inode);
  if (!*res_inode)
    return -EACCES;

  /* truncate file */
  if (flags & O_TRUNC && (*res_inode)->i_op && (*res_inode)->i_op->truncate)
    (*res_inode)->i_op->truncate(*res_inode);

  return 0;
}

/*
 * Make dir system call.
 */
int do_mkdir(int dirfd, const char *pathname, mode_t mode)
{
  struct inode_t *dir;
  const char *basename;
  size_t basename_len;
  int err;

  /* get parent directory */
  dir = dir_namei(dirfd, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;

  /* check name length */
  if (!basename_len) {
    iput(dir);
    return -ENOENT;
  }

  /* mkdir not implemented */
  if (!dir->i_op || !dir->i_op->mkdir) {
    iput(dir);
    return -EPERM;
  }

  /* create directory */
  dir->i_ref++;
  err = dir->i_op->mkdir(dir, basename, basename_len, mode);
  iput(dir);

  return err;
}

/*
 * Link system call (make a new name for a file = hard link).
 */
int do_link(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
  struct inode_t *old_inode, *dir;
  const char *basename;
  size_t basename_len;
  int err;

  /* get old inode */
  old_inode = namei(olddirfd, oldpath, 1);
  if (!old_inode)
    return -ENOENT;

  /* do not allow to rename directory */
  if (S_ISDIR(old_inode->i_mode)) {
    iput(old_inode);
    return -EPERM;
  }

  /* get directory of new file */
  dir = dir_namei(newdirfd, newpath, &basename, &basename_len);
  if (!dir) {
    iput(old_inode);
    return -EACCES;
  }

  /* no name to new file */
  if (!basename_len) {
    iput(old_inode);
    iput(dir);
    return -EPERM;
  }

  /* link not implemented */
  if (!dir->i_op || !dir->i_op->link) {
    iput(old_inode);
    iput(dir);
    return -EPERM;
  }

  /* create link */
  dir->i_ref++;
  err = dir->i_op->link(old_inode, dir, basename, basename_len);

  iput(old_inode);
  iput(dir);
  return err;
}

/*
 * Symbolic link system call.
 */
int do_symlink(const char *target, int newdirfd, const char *linkpath)
{
  struct inode_t *dir;
  const char *basename;
  size_t basename_len;
  int err;

  /* get new parent directory */
  dir = dir_namei(newdirfd, linkpath, &basename, &basename_len);
  if (!dir)
    return -ENOENT;

  /* check directory name */
  if (!basename_len) {
    iput(dir);
    return -ENOENT;
  }

  /* symlink not implemented */
  if (!dir->i_op || !dir->i_op->symlink) {
    iput(dir);
    return -EPERM;
  }

  /* create symbolic link */
  dir->i_ref++;
  err = dir->i_op->symlink(dir, basename, basename_len, target);

  iput(dir);
  return err;
}

/*
 * Unlink system call (remove a file).
 */
int do_unlink(int dirfd, const char *pathname)
{
  struct inode_t *dir;
  const char *basename;
  size_t basename_len;
  int err;

  /* get parent directory */
  dir = dir_namei(dirfd, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;

  /* check name length */
  if (!basename_len) {
    iput(dir);
    return -ENOENT;
  }

  /* unlink not implemented */
  if (!dir->i_op || !dir->i_op->unlink) {
    iput(dir);
    return -EPERM;
  }

  /* unlink file */
  err = dir->i_op->unlink(dir, basename, basename_len);
  return err;
}

/*
 * Rmdir system call (remove a directory).
 */
int do_rmdir(int dirfd, const char *pathname)
{
  struct inode_t *dir;
  const char *basename;
  size_t basename_len;
  int err;

  /* get parent directory */
  dir = dir_namei(dirfd, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;

  /* check name length */
  if (!basename_len) {
    iput(dir);
    return -ENOENT;
  }

  /* rmdir not implemented */
  if (!dir->i_op || !dir->i_op->rmdir) {
    iput(dir);
    return -EPERM;
  }

  /* remove directory */
  err = dir->i_op->rmdir(dir, basename, basename_len);
  return err;
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
