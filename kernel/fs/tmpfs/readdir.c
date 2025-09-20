#include <fs/tmp_fs.h>
#include <stderr.h>

/*
 * Get directory entries system call.
 */
int tmpfs_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct tmpfs_dir_entry de;
	struct dirent64 *dirent;
	int entries_size, ret;

	/* walk through all entries */
	for (entries_size = 0, dirent = (struct dirent64 *) dirp;;) {
		/* read next drectory entry */
		if (tmpfs_file_read(filp, (char *) &de, sizeof(struct tmpfs_dir_entry), &filp->f_pos) != sizeof(struct tmpfs_dir_entry))
			return entries_size;

		/* skip null entries */
		if (!de.d_inode)
			continue;

		/* fill in directory entry */
		ret = filldir(dirent, de.d_name, strlen(de.d_name), de.d_inode, count);
		if (ret) {
			filp->f_pos -= sizeof(struct tmpfs_dir_entry);
			return entries_size;
		}

		/* go to next entry */
		count -= dirent->d_reclen;
		entries_size += dirent->d_reclen;
		dirent = (struct dirent64 *) ((char *) dirent + dirent->d_reclen);
	}

	return entries_size;
}
