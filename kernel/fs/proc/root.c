#include <fs/proc_fs.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>

#define NR_ROOT_DIRENTRY        (sizeof(root_dir) / sizeof(root_dir[0]))

/*
 * Root fs directory.
 */
static struct proc_dir_entry_t root_dir[] = {
    {PROC_ROOT_INO,       1, "."},
    {PROC_ROOT_INO,       2, ".."},
    {PROC_UPTIME_INO,     6, "uptime"},
};

/*
 * Root read dir.
 */
static int proc_root_getdents64(struct file_t *filp, void *dirp, size_t count)
{
  struct dirent64_t *dirent;
  int n, name_len;
  size_t i;

  /* read root dir entries */
  for (i = filp->f_pos, n = 0, dirent = (struct dirent64_t *) dirp; i < NR_ROOT_DIRENTRY; i++, filp->f_pos++) {
    /* check buffer size */
    name_len = root_dir[i].name_len;
    if (count < sizeof(struct dirent64_t) + name_len + 1)
      return n;

    /* set dir entry */
    dirent->d_inode = root_dir[i].ino;
    dirent->d_type = 0;
    memcpy(dirent->d_name, root_dir[i].name, name_len);
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
 * Test if a name matches a directory entry.
 */
static inline int proc_match(const char *name, size_t len, struct proc_dir_entry_t *de)
{
  return len == de->name_len && strncmp(name, de->name, len) == 0;
}

/*
 * Root dir lookup.
 */
static int proc_root_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
  struct proc_dir_entry_t *de;
  size_t i;

  /* dir must be a directory */
  if (!dir)
    return -ENOENT;
  if (!S_ISDIR(dir->i_mode)) {
    iput(dir);
    return -ENOENT;
  }

  /* find matching entry */
  for (i = 0, de = NULL; i < NR_ROOT_DIRENTRY; i++) {
    if (proc_match(name, name_len, &root_dir[i])) {
      de = &root_dir[i];
      break;
    }
  }

  /* no matching entry */
  if (!de) {
    iput(dir);
    return -ENOENT;
  }

  /* get inode */
  *res_inode = iget(dir->i_sb, de->ino);
  if (!*res_inode) {
    iput(dir);
    return -EACCES;
  }

  iput(dir);
  return 0;
}

/*
 * Root file operations.
 */
struct file_operations_t proc_root_fops = {
  .getdents64     = proc_root_getdents64,
};

/*
 * Root inode operations.
 */
struct inode_operations_t proc_root_iops = {
  .fops           = &proc_root_fops,
  .lookup         = proc_root_lookup,
};

