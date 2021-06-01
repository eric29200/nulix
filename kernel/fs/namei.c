#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/*
 * Test if a name matches a directory entry.
 */
static inline int match(const char *name, size_t len, struct minix_dir_entry_t *de)
{
  return strncmp(name, de->name, len) == 0 && (len == MINIX_FILENAME_LEN || de->name[len] == 0);
}

/*
 * Find an entry inside a directory.
 */
static struct buffer_head_t *find_entry(struct inode_t *dir, const char *name, size_t name_len,
                                        struct minix_dir_entry_t **res_dir)
{
  struct minix_dir_entry_t *entries;
  struct buffer_head_t *bh = NULL;
  int nb_entries, i, block_nr;

  /* check name length */
  if (!name_len || name_len > MINIX_FILENAME_LEN)
    return NULL;

  /* get number of entries */
  nb_entries = dir->i_size / sizeof(struct minix_dir_entry_t);

  /* walk through all entries */
  for (i = 0; i < nb_entries; i++) {
    /* read next block if needed */
    if (i % MINIX_DIR_ENTRIES_PER_BLOCK == 0) {
      brelse(bh);
      block_nr = bmap(dir, i / MINIX_DIR_ENTRIES_PER_BLOCK, 0);
      bh = bread(dir->i_dev, block_nr);
      if (!bh)
        return NULL;

      /* get dir entries */
      entries = (struct minix_dir_entry_t *) bh->b_data;
    }

    /* check name */
    if (match(name, name_len, &entries[i % MINIX_DIR_ENTRIES_PER_BLOCK])) {
      *res_dir = &entries[i % MINIX_DIR_ENTRIES_PER_BLOCK];
      return bh;
    }
  }

  /* free buffer */
  brelse(bh);
  return NULL;
}

/*
 * Add an entry to a directory.
 */
static struct buffer_head_t *add_entry(struct inode_t *dir, const char *name, size_t name_len,
                                       struct minix_dir_entry_t **res_dir)
{
  struct minix_dir_entry_t *de;
  struct buffer_head_t *bh;
  int block, i;

  /* check file name length */
  if (!name_len || name_len > MINIX_FILENAME_LEN)
    return NULL;

  /* get first dir block */
  block = dir->i_zone[0];
  if (!block)
    return NULL;

  /* read first dir block */
  bh = bread(dir->i_dev, dir->i_zone[0]);
  if (!bh)
    return NULL;

  /* try to find first free entry */
  i = 0;
  de = (struct minix_dir_entry_t *) bh->b_data;
  while (1) {
    /* read next block */
    if ((char *) de >= bh->b_data + BLOCK_SIZE) {
      /* release previous block */
      brelse(bh);
      bh = NULL;

      /* get or map next block number */
      block = bmap(dir, i / MINIX_DIR_ENTRIES_PER_BLOCK, 1);
      if (!block)
        return NULL;

      /* read block */
      bh = bread(dir->i_dev, block);
      if (!bh) {
        i += MINIX_DIR_ENTRIES_PER_BLOCK;
        continue;
      }

      /* update entries */
      de = (struct minix_dir_entry_t *) bh->b_data;
    }

    /* last entry : create a new one */
    if (i * sizeof(struct minix_dir_entry_t) >= dir->i_size) {
      de->inode = 0;
      dir->i_size = (i + 1) * sizeof(struct minix_dir_entry_t);
      dir->i_dirt = 1;
    }

    /* free entry : exit */
    if (!de->inode) {
      bh->b_dirt = 1;
      memset(de->name, 0, MINIX_FILENAME_LEN);
      memcpy(de->name, name, name_len);
      *res_dir = de;
      return bh;
    }

    i++;
    de++;
  }
}

/*
 * Resolve a symbolic link.
 */
static struct inode_t *follow_link(struct inode_t *inode)
{
  struct buffer_head_t *bh;

  if (!inode || !S_ISLNK(inode->i_mode))
    return inode;

  /* read first link block */
  bh = bread(inode->i_dev, inode->i_zone[0]);
  if (!bh) {
    iput(inode);
    return NULL;
  }

  /* release link inode */
  iput(inode);

  /* resolve target inode */
  inode = namei(AT_FDCWD, bh->b_data, 0);

  /* release link buffer */
  brelse(bh);
  return inode;
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
  struct minix_dir_entry_t *de;
  struct inode_t *dir, *inode;
  struct super_block_t *sb;
  struct buffer_head_t *bh;
  const char *basename;
  size_t basename_len;
  ino_t ino_nr;

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

  /* find inode */
  bh = find_entry(dir, basename, basename_len, &de);
  if (!bh) {
    /* no such entry */
    if (!(flags & O_CREAT)) {
      iput(dir);
      return -ENOENT;
    }

    /* create a new inode */
    inode = new_inode(dir->i_sb);
    if (!inode) {
      iput(dir);
      return -ENOSPC;
    }

    /* set inode */
    inode->i_mode = mode;
    inode->i_dirt = 1;

    /* add new entry to current dir */
    bh = add_entry(dir, basename, basename_len, &de);
    if (!bh) {
      iput(inode);
      iput(dir);
      return -ENOSPC;
    }

    /* set inode entry */
    de->inode = inode->i_ino;
    sb = dir->i_sb;

    /* release current dir inode/block and new inode (to write them on disk) */
    iput(dir);
    iput(inode);

    /* read inode from disk */
    *res_inode = iget(sb, de->inode);
    if (!*res_inode) {
      brelse(bh);
      return -EACCES;
    }

    brelse(bh);
    return 0;
  }

  /* save inode number and release block buffer */
  ino_nr = de->inode;
  sb = dir->i_sb;
  brelse(bh);
  iput(dir);

  /* get inode */
  *res_inode = iget(sb, ino_nr);
  if (!*res_inode)
    return -EACCES;

  /* follow symbolic link */
  *res_inode = follow_link(*res_inode);
  if (!*res_inode)
    return -EACCES;

  /* truncate file */
  if (flags & O_TRUNC)
    minix_truncate(*res_inode);

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

  iput(dir);
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

  iput(dir);
  return err;
}
