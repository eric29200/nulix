#include <fs/fs.h>
#include <fs/buffer.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <stat.h>
#include <string.h>

extern struct minix_super_block_t *root_sb;

/*
 * Find an inode inside a directory.
 */
static struct inode_t *find_entry(struct inode_t *dir, const char *name, size_t name_len)
{
  struct minix_dir_entry_t *entries = NULL;
  struct inode_t *ret = NULL;
  uint32_t nb_entries, i, block_nr;

  /* check name length */
  if (name_len > MINIX_FILENAME_LEN || name_len == 0)
    return NULL;

  /* get number of entries */
  nb_entries = dir->i_size / sizeof(struct minix_dir_entry_t);

  /* walk through all entries */
  for (i = 0; i < nb_entries; i++) {
    /* read next block if needed */
    if (i % MINIX_DIR_ENTRIES_PER_BLOCK == 0) {
      if (entries)
        kfree(entries);

      block_nr = bmap(dir, i / MINIX_DIR_ENTRIES_PER_BLOCK);
      entries = (struct minix_dir_entry_t *) bread(dir->i_dev, block_nr);
    }

    /* check name */
    if (strncmp(name, entries[i % MINIX_DIR_ENTRIES_PER_BLOCK].name, name_len) == 0) {
      ret = read_inode(dir->i_sb, entries[i % MINIX_DIR_ENTRIES_PER_BLOCK].inode);
      break;
    }
  }

  /* free buffer */
  kfree(entries);

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

  /* absolute path only */
  if (*pathname != '/')
    return NULL;
  pathname++;

  /* set inode to root */
  inode = root_sb->s_imount;

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

    /* free curent inode (except root inode) */
    if (inode != inode->i_sb->s_imount)
      kfree(inode);

    /* go to next inode */
    inode = next_inode;
  }

  *basename = name;
  *basename_len = name_len;
  return inode;
err:
  /* free inode (except root inode) */
  if (inode && inode != inode->i_sb->s_imount)
    kfree(inode);
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

  /* find inode */
  ret = find_entry(dir, basename, basename_len);

  /* free directory */
  kfree(dir);
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

  /* find inode */
  *inode = find_entry(dir, basename, basename_len);
  if (!*inode) {
    kfree(dir);
    return -ENOENT;
  }

  /* free directory */
  kfree(dir);
  return 0;
}
