#include <fs/dev_fs.h>
#include <stderr.h>

/*
 * Get directory entries system call.
 */
int devfs_getdents64(struct file_t *filp, void *dirp, size_t count)
{
	struct devfs_dir_entry_t de;
	struct dirent64_t *dirent;
	int entries_size, ret;

	/* walk through all entries */
	for (entries_size = 0, dirent = (struct dirent64_t *) dirp;;) {
		/* read next drectory entry */
		if (devfs_file_read(filp, (char *) &de, sizeof(struct devfs_dir_entry_t)) != sizeof(struct devfs_dir_entry_t))
			return entries_size;

		/* skip null entries */
		if (!de.d_inode)
			continue;

		/* fill in directory entry */
		ret = filldir(dirent, de.d_name, strlen(de.d_name), de.d_inode, count);
		if (ret) {
			filp->f_pos -= sizeof(struct devfs_dir_entry_t);
			return entries_size;
		}

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64_t *) ((char *) dirent + dirent->d_reclen);
	}

	return entries_size;
}
