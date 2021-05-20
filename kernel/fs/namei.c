#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/* root super block */
extern struct minix_super_block_t *root_sb;

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
static struct buffer_head_t *find_entryi(struct inode_t *dir, ino_t ino, struct minix_dir_entry_t **res_dir)
{
  struct minix_dir_entry_t *entries;
  struct buffer_head_t *bh = NULL;
  int nb_entries, i, block_nr;

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
    if (entries[i % MINIX_DIR_ENTRIES_PER_BLOCK].inode == ino) {
      *res_dir = &entries[i % MINIX_DIR_ENTRIES_PER_BLOCK];
      return bh;
    }
  }

  /* free buffer */
  brelse(bh);
  return NULL;
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
 * Resolve a path name to the inode of the top most directory.
 */
static struct inode_t *dir_namei(int dirfd, const char *pathname, const char **basename, size_t *basename_len)
{
  struct minix_super_block_t *sb;
  struct minix_dir_entry_t *de;
  struct buffer_head_t *bh;
  struct inode_t *inode;
  const char *name;
  size_t name_len;
  ino_t ino_nr;

  /* absolute or relative path */
  if (*pathname == '/') {
    inode = root_sb->s_imount;
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

    /* find entry */
    bh = find_entry(inode, name, name_len, &de);
    if (!bh) {
      iput(inode);
      return NULL;
    }

    /* save inode number and release block buffer*/
    ino_nr = de->inode;
    sb = inode->i_sb;
    brelse(bh);
    iput(inode);

    /* get next inode */
    inode = iget(sb, ino_nr);
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
struct inode_t *namei(int dirfd, const char *pathname)
{
  struct minix_super_block_t *sb;
  struct minix_dir_entry_t *de;
  struct buffer_head_t *bh;
  struct inode_t *dir;
  const char *basename;
  size_t basename_len;
  ino_t ino_nr;

  /* find directory */
  dir = dir_namei(dirfd, pathname, &basename, &basename_len);
  if (!dir)
    return NULL;

  /* special case : '/' */
  if (!basename_len)
    return dir;

  /* find entry */
  bh = find_entry(dir, basename, basename_len, &de);
  if (!bh) {
    iput(dir);
    return NULL;
  }

  /* save inode number and release buffer */
  ino_nr = de->inode;
  sb = dir->i_sb;
  brelse(bh);
  iput(dir);

  /* read inode */
  return iget(sb, ino_nr);
}

/*
 * Resolve and open a path name.
 */
int open_namei(int dirfd, const char *pathname, int flags, mode_t mode, struct inode_t **res_inode)
{
  struct minix_super_block_t *sb;
  struct minix_dir_entry_t *de;
  struct inode_t *dir, *inode;
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
    inode = new_inode();
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

    /* release current dir inode/block and new inode (to write them on disk) */
    iput(dir);
    iput(inode);

    /* read inode from disk */
    *res_inode = iget(dir->i_sb, de->inode);
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

  /* truncate file */
  if (flags & O_TRUNC)
    truncate(*res_inode);

  return 0;
}

/*
 * Check if a directory is empty.
 */
static int empty_dir(struct inode_t *inode)
{
  struct minix_dir_entry_t *de;
  struct buffer_head_t *bh;
  int len, i, block;

  /* check if inode is a directory */
  if (!inode || S_ISREG(inode->i_mode))
    return 0;

  /* get directory length */
  len = inode->i_size / sizeof(struct minix_dir_entry_t);
  if (len < 2 || !inode->i_zone[0])
    return 0;

  /* get first zone */
  bh = bread(inode->i_dev, inode->i_zone[0]);
  if (!bh)
    return 0;

  /* skip 2 first entries (. and ..) */
  de = (struct minix_dir_entry_t *) bh->b_data;
  de += 2;
  i = 2;

  /* go through each entry */
  while (i < len) {
    /* read next block */
    if ((void *) de >= (void *) (bh->b_data + BLOCK_SIZE)) {
      /* release previous block buffer */
      brelse(bh);

      /* get next block buffer */
      block = bmap(inode, i / MINIX_DIR_ENTRIES_PER_BLOCK, 0);
      bh = bread(inode->i_dev, block);
      if (!bh)
        return 0;

      /* update entry */
      de = (struct minix_dir_entry_t *) bh->b_data;
    }

    /* found an entry : directory is not empty */
    if (de->inode) {
      brelse(bh);
      return 0;
    }

    /* go to next dir entry */
    de++;
    i++;
  }

  /* no entries found : directory is empty */
  brelse(bh);
  return 1;
}

/*
 * Make dir system call.
 */
int do_mkdir(int dirfd, const char *pathname, mode_t mode)
{
  struct minix_dir_entry_t *de;
  struct inode_t *dir, *inode;
  struct buffer_head_t *bh;
  const char *basename;
  size_t basename_len;

  /* get parent directory */
  dir = dir_namei(dirfd, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;

  /* check if file exists */
  bh = find_entry(dir, basename, basename_len, &de);
  if (bh) {
    brelse(bh);
    iput(dir);
    return -EEXIST;
  }

  /* allocate a new inode */
  inode = new_inode();
  if (!inode) {
    iput(dir);
    return -ENOSPC;
  }

  /* set inode */
  inode->i_size = sizeof(struct minix_dir_entry_t) * 2;
  inode->i_nlinks = 2;
  inode->i_time = CURRENT_TIME;
  inode->i_dirt = 1;
  inode->i_mode = S_IFDIR | (mode & ~current_task->umask & 0777);
  inode->i_uid = current_task->uid;
  inode->i_gid = current_task->gid;
  inode->i_zone[0] = new_block();
  if (!inode->i_zone[0]) {
    iput(dir);
    iput(inode);
    return -ENOSPC;
  }

  /* read first block */
  bh = bread(inode->i_dev, inode->i_zone[0]);
  if (!bh) {
    iput(dir);
    free_block(inode->i_zone[0]);
    iput(inode);
  }

  /* add entry '.' */
  de = (struct minix_dir_entry_t *) bh->b_data;
  de->inode = inode->i_ino;
  strcpy(de->name, ".");

  /* add entry '..' */
  de++;
  de->inode = dir->i_ino;
  strcpy(de->name, "..");

  /* release first block */
  bh->b_dirt = 1;
  brelse(bh);

  /* add entry to parent dir */
  bh = add_entry(dir, basename, basename_len, &de);
  if (!bh) {
    iput(dir);
    free_block(inode->i_zone[0]);
    iput(inode);
    return -ENOSPC;
  }

  /* set inode entry */
  de->inode = inode->i_ino;

  /* update directory links and mark directory dirty */
  dir->i_nlinks++;
  dir->i_dirt = 1;

  /* release inode */
  iput(dir);
  iput(inode);
  brelse(bh);

  return 0;
}

/*
 * Link system call (make a new name for a file = hard link).
 */
int do_link(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
  struct inode_t *old_inode, *dir;
  struct minix_dir_entry_t *de;
  struct buffer_head_t *bh;
  const char *basename;
  size_t basename_len;

  /* get old inode */
  old_inode = namei(olddirfd, oldpath);
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

  /* check if new file already exist */
  bh = find_entry(dir, basename, basename_len, &de);
  if (bh) {
    brelse(bh);
    iput(dir);
    iput(old_inode);
    return -EEXIST;
  }

  /* add entry */
  bh = add_entry(dir, basename, basename_len, &de);
  if (!bh) {
    iput(dir);
    iput(old_inode);
    return -ENOSPC;
  }

  /* update directory entry */
  de->inode = old_inode->i_ino;
  bh->b_dirt = 1;
  brelse(bh);

  /* update old inode */
  old_inode->i_nlinks++;
  old_inode->i_dirt = 1;
  iput(old_inode);

  return 0;
}

/*
 * Symbolic link system call.
 */
int do_symlink(const char *target, int newdirfd, const char *linkpath)
{
  struct inode_t *dir, *inode;
  struct minix_dir_entry_t *de;
  struct buffer_head_t *bh;
  const char *basename;
  size_t basename_len;
  int i;

  /* get new parent directory */
  dir = dir_namei(newdirfd, linkpath, &basename, &basename_len);
  if (!dir || !basename_len)
    return -ENOENT;

  /* allocate a new inode */
  inode = new_inode();
  if (!inode) {
    iput(dir);
    return -ENOSPC;
  }

  /* set new inode */
  inode->i_mode = S_IFLNK | (0777 & ~current_task->umask);
  inode->i_dirt = 1;

  /* create first block */
  inode->i_zone[0] = new_block();
  if (!inode->i_zone[0]) {
    iput(dir);
    inode->i_nlinks--;
    iput(inode);
    return -ENOSPC;
  }

  /* read first block */
  bh = bread(inode->i_dev, inode->i_zone[0]);
  if (!bh) {
    iput(dir);
    inode->i_nlinks--;
    iput(inode);
    return -ENOSPC;
  }

  /* write file name on first block */
  for (i = 0; target[i] && i < BLOCK_SIZE - 1; i++)
    bh->b_data[i] = target[i];
  bh->b_data[i] = 0;
  bh->b_dirt = 1;
  brelse(bh);

  /* update inode size */
  inode->i_size = i;
  inode->i_dirt = 1;

  /* check if file exists */
  bh = find_entry(dir, basename, basename_len, &de);
  if (bh) {
    inode->i_nlinks--;
    iput(inode);
    brelse(bh);
    iput(dir);
    return -EEXIST;
  }

  /* add entry */
  bh = add_entry(dir, basename, basename_len, &de);
  if (!bh) {
    inode->i_nlinks--;
    iput(inode);
    iput(dir);
    return -ENOSPC;
  }

  /* set inode entry */
  de->inode = inode->i_ino;
  bh->b_dirt = 1;

  /* release inode */
  brelse(bh);
  iput(dir);
  iput(inode);

  return 0;
}

/*
 * Unlink system call (remove a file).
 */
int do_unlink(int dirfd, const char *pathname)
{
  struct minix_dir_entry_t *de;
  struct inode_t *dir, *inode;
  struct buffer_head_t *bh;
  const char *basename;
  size_t basename_len;

  /* get parent directory */
  dir = dir_namei(dirfd, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;

  /* check if file exists */
  bh = find_entry(dir, basename, basename_len, &de);
  if (!bh) {
    iput(dir);
    return -ENOENT;
  }

  /* get inode */
  inode = iget(dir->i_sb, de->inode);
  if (!inode) {
    iput(dir);
    brelse(bh);
    return -ENOENT;
  }

  /* remove regular files only */
  if (!S_ISREG(inode->i_mode)) {
    iput(inode);
    iput(dir);
    brelse(bh);
    return -EPERM;
  }

  /* reset entry */
  memset(de, 0, sizeof(struct minix_dir_entry_t));
  bh->b_dirt = 1;
  brelse(bh);

  /* update inode */
  inode->i_nlinks--;
  inode->i_dirt = 1;
  iput(inode);
  iput(dir);
  return 0;
}

/*
 * Rmdir system call (remove a directory).
 */
int do_rmdir(int dirfd, const char *pathname)
{
  struct minix_dir_entry_t *de;
  struct inode_t *dir, *inode;
  struct buffer_head_t *bh;
  const char *basename;
  size_t basename_len;

  /* get parent directory */
  dir = dir_namei(dirfd, pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;

  /* check if file exists */
  bh = find_entry(dir, basename, basename_len, &de);
  if (!bh) {
    iput(dir);
    return -ENOENT;
  }

  /* get inode */
  inode = iget(dir->i_sb, de->inode);
  if (!inode) {
    iput(dir);
    brelse(bh);
    return -ENOENT;
  }

  /* remove directories only (and avoid to remove ".") */
  if (inode == dir || !S_ISDIR(inode->i_mode)) {
    iput(inode);
    iput(dir);
    brelse(bh);
    return -EPERM;
  }

  /* directory must be empty */
  if (!empty_dir(inode)) {
    iput(inode);
    iput(dir);
    brelse(bh);
    return -EPERM;
  }

  /* reset entry */
  memset(de, 0, sizeof(struct minix_dir_entry_t));
  bh->b_dirt = 1;
  brelse(bh);

  /* update dir */
  dir->i_nlinks--;
  dir->i_dirt = 1;

  /* update inode */
  inode->i_nlinks = 0;
  inode->i_dirt = 1;

  /* release dir and inode */
  iput(inode);
  iput(dir);
  return 0;
}

/*
 * Get current working dir system call.
 */
int do_getcwd(char *buf, size_t size)
{
  struct inode_t *inode, *parent;
  struct minix_dir_entry_t *de;
  struct buffer_head_t *bh;
  char *end;
  int len;
  size_t n;

  if (!current_task->cwd)
    return -EINVAL;

  /* start with current working directory */
  inode = iget(current_task->cwd->i_sb, current_task->cwd->i_ino);
  if (!inode)
    return -ENOENT;

  for (n = 0, end = buf + size;;) {
    /* get parent inode */
    parent = iget_parent(inode);
    if (!parent) {
      iput(inode);
      return -ENOENT;
    }

    /* same parent : break */
    if (parent->i_ino == inode->i_ino) {
      iput(inode);
      iput(parent);
      break;
    }

    /* find matching entry in parent */
    bh = find_entryi(parent, inode->i_ino, &de);
    if (!bh)
      return -ENOENT;

    /* check name sizes */
    len = strnlen(de->name, MINIX_FILENAME_LEN);
    end -= len + 1;
    n += len + 1;
    if (n > size) {
      iput(parent);
      iput(inode);
      brelse(bh);
      return -ERANGE;
    }

    /* copy de->name in buffer */
    *end = '/';
    memcpy(end + 1, de->name, len);

    /* release inode and buffer */
    iput(inode);
    brelse(bh);

    /* go to parent */
    inode = parent;
  }

  /* add root / */
  if (n == 0) {
    *--end = '/';
    n++;
  }

  /* shift path */
  memcpy(buf, end, n);
  buf[n] = 0;

  return n;
}
