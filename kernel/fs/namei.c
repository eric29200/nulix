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
static struct inode_t *dir_namei(const char *pathname, const char **basename, size_t *basename_len)
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
  } else {
    inode = current_task->cwd;
  }

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
struct inode_t *namei(const char *pathname)
{
  struct minix_super_block_t *sb;
  struct minix_dir_entry_t *de;
  struct buffer_head_t *bh;
  struct inode_t *dir;
  const char *basename;
  size_t basename_len;
  ino_t ino_nr;

  /* find directory */
  dir = dir_namei(pathname, &basename, &basename_len);
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
int open_namei(const char *pathname, int flags, mode_t mode, struct inode_t **res_inode)
{
  struct minix_super_block_t *sb;
  struct minix_dir_entry_t *de;
  struct inode_t *dir, *inode;
  struct buffer_head_t *bh;
  const char *basename;
  size_t basename_len;
  ino_t ino_nr;

  /* find directory */
  dir = dir_namei(pathname, &basename, &basename_len);
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
    brelse(bh);

    /* read inode from disk */
    *res_inode = iget(dir->i_sb, de->inode);
    if (!*res_inode)
      return -EACCES;

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
 * Make dir system call.
 */
int do_mkdir(const char *pathname, mode_t mode)
{
  struct minix_dir_entry_t *de;
  struct inode_t *dir, *inode;
  struct buffer_head_t *bh;
  const char *basename;
  size_t basename_len;

  /* get parent directory */
  dir = dir_namei(pathname, &basename, &basename_len);
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
 * Unlink system call (remove a file).
 */
int do_unlink(const char *pathname)
{
  struct minix_dir_entry_t *de;
  struct inode_t *dir, *inode;
  struct buffer_head_t *bh;
  const char *basename;
  size_t basename_len;

  /* get parent directory */
  dir = dir_namei(pathname, &basename, &basename_len);
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
