#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <stat.h>
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
 * Find an inode inside a directory.
 */
static struct inode_t *find_entry(struct inode_t *dir, const char *name, size_t name_len)
{
  struct minix_dir_entry_t *entry = NULL;
  uint32_t nb_entries, i, block_nr;
  struct buffer_head_t *bh = NULL;
  struct inode_t *ret = NULL;

  /* check name length */
  if (name_len > MINIX_FILENAME_LEN || name_len == 0)
    return NULL;

  /* get number of entries */
  nb_entries = dir->i_size / sizeof(struct minix_dir_entry_t);

  /* walk through all entries */
  for (i = 0; i < nb_entries; i++) {
    /* read next block if needed */
    if (i % MINIX_DIR_ENTRIES_PER_BLOCK == 0) {
      brelse(bh);
      block_nr = bmap(dir, i / MINIX_DIR_ENTRIES_PER_BLOCK);
      bh = bread(dir->i_dev, block_nr);
      if (!bh)
        return NULL;
    }

    /* check name */
    entry = &((struct minix_dir_entry_t *) bh->b_data)[i % MINIX_DIR_ENTRIES_PER_BLOCK];
    if (match(name, name_len, entry)) {
      ret = iget(dir->i_sb, entry->inode);
      break;
    }
  }

  /* free buffer */
  brelse(bh);

  return ret;
}

/*
 * Resolve a path name to the inode of the top most directory.
 */
static struct inode_t *dir_namei(const char *pathname, const char **basename, size_t *basename_len)
{
  struct inode_t *inode, *next_inode;
  const char *name;
  size_t name_len;

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
    if (!S_ISDIR(inode->i_mode))
      goto err;

    /* compute next path name */
    name = pathname;
    for (name_len = 0; *pathname && *pathname++ != '/'; name_len++);

    /* end : return current inode */
    if (!*pathname)
      break;

    /* get matching inode */
    next_inode = find_entry(inode, name, name_len);
    if (!next_inode)
      goto err;

    /* free curent inode */
    iput(inode);

    /* go to next inode */
    inode = next_inode;
  }

  *basename = name;
  *basename_len = name_len;
  return inode;
err:
  /* free inode */
  iput(inode);
  return NULL;
}

/*
 * Get an inode.
 */
struct inode_t *namei(const char *pathname)
{
  struct inode_t *dir, *ret;
  const char *basename;
  size_t basename_len;

  /* find directory */
  dir = dir_namei(pathname, &basename, &basename_len);
  if (!dir)
    return NULL;

  /* special case : '/' */
  if (!basename_len)
    return dir;

  /* find inode */
  ret = find_entry(dir, basename, basename_len);

  /* free directory */
  iput(dir);
  return ret;
}

/*
 * Open a file.
 */
int open_namei(const char *pathname, struct inode_t **inode)
{
  struct inode_t *dir;
  const char *basename;
  size_t basename_len;

  /* find directory */
  dir = dir_namei(pathname, &basename, &basename_len);
  if (!dir)
    return -ENOENT;

  /* open a directory */
  if (!basename_len) {
    *inode = dir;
    return 0;
  }

  /* find inode */
  *inode = find_entry(dir, basename, basename_len);
  if (!*inode) {
    iput(dir);
    return -ENOENT;
  }

  /* free directory */
  iput(dir);
  return 0;
}

/*
 * Make dir system call.
 */
int do_mkdir(const char *pathname)
{
  return 0;
}
