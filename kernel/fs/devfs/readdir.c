#include <fs/dev_fs.h>
#include <stderr.h>

/*
 * Get directory entries system call.
 */
int devfs_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct devfs_dir_entry_t de;
	struct dirent64_t *dirent;
	int entries_size;
	size_t name_len;

	/* walk through all entries */
	for (entries_size = 0, dirent = (struct dirent64_t *) dirp;;) {
		/* read next drectory entry */
		if (devfs_file_read(filp, (char *) &de, sizeof(struct devfs_dir_entry_t)) != sizeof(struct devfs_dir_entry_t))
			return entries_size;

		/* skip null entries */
		if (!de.d_inode)
			continue;

		/* not enough space to fill in next dir entry */
		name_len = strlen(de.d_name);
		if (count < sizeof(struct dirent64_t) + name_len + 1) {
			filp->f_pos -= sizeof(struct devfs_dir_entry_t);
			return entries_size;
		}

		/* fill in dirent */
		dirent->d_inode = de.d_inode;
		dirent->d_off = 0;
		dirent->d_type = 0;
		dirent->d_reclen = sizeof(struct dirent64_t) + name_len + 1;
		memcpy(dirent->d_name, de.d_name, name_len);
		dirent->d_name[name_len] = 0;

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
	}

	return entries_size;
}
