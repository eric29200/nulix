#include <fs/proc_fs.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>

#include <proc/sched.h>
#include <stdio.h>

#define NR_BASE_DIRENTRY        (sizeof(base_dir) / sizeof(base_dir[0]))
#define FAKE_INODE(pid, ino)    (((pid) << 16) | (ino))

/*
 * Base process directory.
 */
static struct proc_dir_entry_t base_dir[] = {
    {0,       4, "stat"},
};

/*
 * Read process stat.
 */
static int proc_stat_read(struct file_t *filp, char *buf, int count)
{
  char tmp_buf[256];
  size_t len;
  pid_t pid;

  /* get process pid */
  pid = filp->f_inode->i_ino >> 16;

  /* print pid in temporary buffer */
  len = sprintf(tmp_buf, "%d\n", pid);

  /* file position after end */
  if (filp->f_pos >= len)
    return 0;

  /* update count */
  if (filp->f_pos + count > len)
    count = len - filp->f_pos;

  /* copy content to user buffer and update file position */
  memcpy(buf, tmp_buf + filp->f_pos, count);
  filp->f_pos += count;

  return count;
}

/*
 * Stat file operations.
 */
struct file_operations_t proc_stat_fops = {
  .read           = proc_stat_read,
};

/*
 * Stat inode operations.
 */
struct inode_operations_t proc_stat_iops = {
  .fops           = &proc_stat_fops,
};


/*
 * Get directory entries.
 */
static int proc_base_getdents64(struct file_t *filp, void *dirp, size_t count)
{
  struct dirent64_t *dirent;
  int name_len, n;
  size_t i;

  /* read root dir entries */
  for (i = filp->f_pos, n = 0, dirent = (struct dirent64_t *) dirp; i < NR_BASE_DIRENTRY; i++, filp->f_pos++) {
    /* check buffer size */
    name_len = base_dir[i].name_len;
    if (count < sizeof(struct dirent64_t) + name_len + 1)
      return n;

    /* set dir entry */
    dirent->d_inode = base_dir[i].ino;
    dirent->d_type = 0;
    memcpy(dirent->d_name, base_dir[i].name, name_len);
    dirent->d_name[name_len] = 0;

    /* set dir entry size */
    dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;

    /* go to next dir entry */
    count -= dirent->d_reclen;
    n += dirent->d_reclen;
    dirent = (struct dirent64_t *) ((void *) dirent + dirent->d_reclen);
  }

  return n;
}

/*
 * Lookup for a file.
 */
static int proc_base_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
  struct proc_dir_entry_t *de;
  ino_t ino;
  size_t i;

  /* dir must be a directory */
  if (!dir)
    return -ENOENT;
  if (!S_ISDIR(dir->i_mode)) {
    iput(dir);
    return -ENOENT;
  }

  /* find matching entry */
  for (i = 0, de = NULL; i < NR_BASE_DIRENTRY; i++) {
    if (proc_match(name, name_len, &base_dir[i])) {
      de = &base_dir[i];
      break;
    }
  }

  /* no such entry */
  if (!de) {
    iput(dir);
    return -ENOENT;
  }

  /* create a fake inode */
  ino = FAKE_INODE(dir->i_ino - PROC_BASE_INO, de->ino);

  /* get inode */
  *res_inode = iget(dir->i_sb, ino);
  if (!*res_inode) {
    iput(dir);
    return -EACCES;
  }

  /* stat file */
  if (de->ino == 0) {
    (*res_inode)->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
    (*res_inode)->i_op = &proc_stat_iops;
    goto out;
  }

out:
  iput(dir);
  return 0;
}

/*
 * Process file operations.
 */
struct file_operations_t proc_base_fops = {
  .getdents64     = proc_base_getdents64,
};

/*
 * Process inode operations.
 */
struct inode_operations_t proc_base_iops = {
  .fops           = &proc_base_fops,
  .lookup         = proc_base_lookup,
};

