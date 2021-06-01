#include <fs/minix_fs.h>
#include <string.h>

/*
 * Get directory entries system call.
 */
int minix_getdents(struct file_t *filp, struct dirent_t *dirent, uint32_t count)
{
  struct minix_dir_entry_t de;
  int entries_size;
  size_t name_len;

  for (entries_size = 0;;) {
    /* read minix dir entry */
    if (minix_file_read(filp, (char *) &de, sizeof(struct minix_dir_entry_t)) != sizeof(struct minix_dir_entry_t))
      return entries_size;

    /* skip null entries */
    if (de.inode == 0)
      continue;

    /* not enough space to fill in next dir entry : break */
    name_len = strlen(de.name);
    if (count < sizeof(struct dirent_t) + name_len + 1) {
      filp->f_pos -= sizeof(struct minix_dir_entry_t);
      return entries_size;
    }

    /* fill in dirent */
    dirent->d_inode = de.inode;
    dirent->d_off = 0;
    dirent->d_reclen = sizeof(struct dirent_t) + name_len + 1;
    dirent->d_type = 0;
    memcpy(dirent->d_name, de.name, name_len);
    dirent->d_name[name_len] = 0;

    /* go to next entry */
    count -= dirent->d_reclen;
    entries_size += dirent->d_reclen;
    dirent = (struct dirent_t *) ((char *) dirent + dirent->d_reclen);
  }

  return entries_size;
}

/*
 * Get directory entries system call.
 */
int minix_getdents64(struct file_t *filp, void *dirp, size_t count)
{
  struct minix_dir_entry_t de;
  struct dirent64_t *dirent;
  size_t name_len;
  int entries_size;

  for (entries_size = 0, dirent = (struct dirent64_t *) dirp;;) {
    /* read minix dir entry */
    if (minix_file_read(filp, (char *) &de, sizeof(struct minix_dir_entry_t)) != sizeof(struct minix_dir_entry_t))
      return entries_size;

    /* skip null entries */
    if (de.inode == 0)
      continue;

    /* not enough space to fill in next dir entry : break */
    name_len = strlen(de.name);
    if (count < sizeof(struct dirent64_t) + name_len + 1) {
      filp->f_pos -= sizeof(struct minix_dir_entry_t);
      return entries_size;
    }

    /* fill in dirent */
    dirent->d_inode = de.inode;
    dirent->d_off = 0;
    dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;
    dirent->d_type = 0;
    memcpy(dirent->d_name, de.name, name_len);
    dirent->d_name[name_len] = 0;

    /* go to next entry */
    count -= dirent->d_reclen;
    entries_size += dirent->d_reclen;
    dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
  }

  return entries_size;
}
