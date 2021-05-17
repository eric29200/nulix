#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stderr.h>
#include <string.h>

/*
 * Get directory entries system call.
 */
int do_getdents(int fd, struct dirent_t *dirent, uint32_t count)
{
  struct minix_dir_entry_t *de;
  char *buf, *ibuf;
  size_t name_len;
  int entries_size;
  ssize_t n;

  /* check fd */
  if (fd < 0 || fd >= NR_OPEN)
    return -EINVAL;

  if (count == 0)
    return 0;

  /* allocate a buffer */
  buf = (char *) kmalloc(count);
  if (!buf)
    return -ENOMEM;

  /* read file */
  n = do_read(fd, buf, count);
  if (n <= 0)
    return n;

  for (ibuf = buf, entries_size = 0; ibuf - buf < n;) {
    de = (struct minix_dir_entry_t *) ibuf;

    /* fill in dirent */
    name_len = strlen(de->name);
    dirent->d_inode = de->inode;
    dirent->d_off = 0;
    dirent->d_reclen = sizeof(struct dirent_t) + name_len + 1;
    dirent->d_type = 0;
    memcpy(dirent->d_name, de->name, name_len);
    dirent->d_name[name_len] = 0;

    /* go to next entry */
    entries_size += dirent->d_reclen;
    ibuf += sizeof(struct minix_dir_entry_t);
    dirent = (struct dirent_t *) ((char *) dirent + dirent->d_reclen);
  }

  kfree(buf);
  return entries_size;
}

/*
 * Get directory entries system call.
 */
int do_getdents64(int fd, void *dirp, size_t count)
{
  struct minix_dir_entry_t *de;
  struct dirent64_t *dirent;
  char *buf, *ibuf;
  size_t name_len;
  int entries_size;
  ssize_t n;

  /* check fd */
  if (fd < 0 || fd >= NR_OPEN)
    return -EINVAL;

  if (count == 0)
    return 0;

  /* allocate a buffer */
  buf = (char *) kmalloc(count);
  if (!buf)
    return -ENOMEM;

  /* read file */
  n = do_read(fd, buf, count);
  if (n <= 0)
    return n;

  for (ibuf = buf, dirent = dirp, entries_size = 0; ibuf - buf < n;) {
    de = (struct minix_dir_entry_t *) ibuf;

    /* fill in dirent */
    name_len = strlen(de->name);
    dirent->d_inode = de->inode;
    dirent->d_off = 0;
    dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;
    dirent->d_type = 0;
    memcpy(dirent->d_name, de->name, name_len);
    dirent->d_name[name_len] = 0;

    /* go to next entry */
    entries_size += dirent->d_reclen;
    ibuf += sizeof(struct minix_dir_entry_t);
    dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
  }

  kfree(buf);
  return entries_size;
}
