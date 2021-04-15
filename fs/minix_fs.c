#include <fs/minix_fs.h>
#include <fs/buffer.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>
#include <stat.h>
#include <string.h>

/* root fs super block */
static struct minix_super_block_t *root_sb = NULL;

/*
 * Read an inode.
 */
static struct inode_t *read_inode(struct minix_super_block_t *sb, ino_t ino)
{
  struct minix_inode_t *minix_inode;
  struct inode_t *inode;
  uint32_t block, i, j;

  /* allocate a new inode */
  inode = (struct inode_t *) kmalloc(sizeof(struct inode_t));
  if (!inode)
    return NULL;

  /* read minix inode */
  block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks + (ino - 1) / MINIX_INODES_PER_BLOCK;
  minix_inode = (struct minix_inode_t *) bread(sb->s_dev, block);
  i = (ino - 1) % MINIX_INODES_PER_BLOCK;

  /* fill in memory inode */
  inode->i_mode = minix_inode[i].i_mode;
  inode->i_uid = minix_inode[i].i_uid;
  inode->i_size = minix_inode[i].i_size;
  inode->i_time = minix_inode[i].i_time;
  inode->i_gid = minix_inode[i].i_gid;
  inode->i_nlinks = minix_inode[i].i_nlinks;
  for (j = 0; j < 9; j++)
    inode->i_zone[j] = minix_inode[i].i_zone[j];
  inode->i_ino = ino;
  inode->i_sb = sb;
  inode->i_dev = sb->s_dev;

  /* free minix inode */
  kfree(minix_inode);

  return inode;
}

/*
 * Find an inode inside a directory.
 */
static struct inode_t *find_entry(struct inode_t *dir, const char *name, size_t name_len)
{
  struct minix_dir_entry_t *entries = NULL;
  struct inode_t *ret = NULL;
  uint32_t nb_entries, i;

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

      entries = (struct minix_dir_entry_t *) bread(dir->i_dev, dir->i_zone[i / MINIX_DIR_ENTRIES_PER_BLOCK]);
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
 * Open a file.
 */
static int open_namei(const char *pathname, struct inode_t **inode)
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

/*
 * Open system call.
 */
int sys_open(const char *pathname)
{
  struct inode_t *inode;
  struct file_t *file;
  int fd, ret;

  /* find a free slot in current process */
  for (fd = 0; fd < NR_OPEN; fd++)
    if (!current_task->filp[fd])
      break;

  /* no slots : exit */
  if (fd >= NR_OPEN)
    return -EINVAL;

  /* open file */
  ret = open_namei(pathname, &inode);
  if (ret != 0)
    return ret;

  /* allocate a new file */
  file = (struct file_t *) kmalloc(sizeof(struct file_t));
  if (!file) {
    kfree(inode);
    return -ENOMEM;
  }

  /* set file */
  file->f_mode = inode->i_mode;
  file->f_inode = inode;
  file->f_pos = 0;
  current_task->filp[fd] = file;

  return fd;
}

/*
 * Close system call.
 */
int sys_close(int fd)
{
  struct file_t *filp;

  if (fd < 0 || fd >= NR_OPEN)
    return -EINVAL;

  filp = current_task->filp[fd];
  if (!filp)
    return -ENOMEM;

  current_task->filp[fd] = NULL;
  if (filp->f_inode)
    kfree(filp->f_inode);
  kfree(filp);

  return 0;
}

/*
 * Mount root device.
 */
int mount_root(struct ata_device_t *dev)
{
  struct minix_super_block_t *sb;
  uint32_t block;
  int i, ret;

  /* read super block */
  sb = (struct minix_super_block_t *) bread(dev, 1);
  root_sb = sb;

  /* check magic number */
  if (sb->s_magic != MINIX_SUPER_MAGIC) {
    ret = EINVAL;
    goto err_magic;
  }

  /* set super block device */
  sb->s_dev = dev;
  sb->s_imap = NULL;
  sb->s_zmap = NULL;

  /* allocate imap blocks */
  sb->s_imap = (char **) kmalloc(sizeof(char *) * sb->s_imap_blocks);
  if (!sb->s_imap) {
    ret = ENOMEM;
    goto err_map;
  }

  /* allocate zmap blocks */
  sb->s_zmap = (char **) kmalloc(sizeof(char *) * sb->s_zmap_blocks);
  if (!sb->s_imap) {
    ret = ENOMEM;
    goto err_map;
  }

  /* reset maps */
  for (i = 0; i < sb->s_imap_blocks; i++)
    sb->s_imap[i] = NULL;
  for (i = 0; i < sb->s_zmap_blocks; i++)
    sb->s_zmap[i] = NULL;

  /* read imap blocks */
  block = 2;
  for (i = 0; i < sb->s_imap_blocks; i++, block++) {
    sb->s_imap[i] = bread(dev, block);
    if (!sb->s_imap[i]) {
      ret = ENOMEM;
      goto err_map;
    }
  }

  /* read zmap blocks */
  for (i = 0; i < sb->s_zmap_blocks; i++, block++) {
    sb->s_zmap[i] = bread(dev, block);
    if (!sb->s_zmap[i]) {
      ret = ENOMEM;
      goto err_map;
    }
  }

  /* read root inode */
  sb->s_imount = read_inode(sb, 1);
  if (!sb->s_imount) {
    ret = EINVAL;
    goto err_root_inode;
  }

  return 0;
err_root_inode:
  printf("[Minix-fs] Can't read root inode\n");
  if (sb->s_imount)
    kfree(sb->s_imount);
  goto release_map;
err_map:
  printf("[Minix-fs] Can't read imap and zmap\n");
release_map:
  if (sb->s_imap)
    for (i = 0; i < sb->s_imap_blocks; i++)
      if (sb->s_imap[i])
        kfree(sb->s_imap[i]);
  if (sb->s_zmap)
    for (i = 0; i < sb->s_zmap_blocks; i++)
      if (sb->s_zmap[i])
        kfree(sb->s_zmap[i]);
  goto release_sb;
err_magic:
  printf("[Minix-fs] Bad magic number\n");
release_sb:
  kfree(sb);
  return ret;
}
