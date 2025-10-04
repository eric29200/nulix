#include <fs/tmp_fs.h>
#include <stderr.h>

/*
 * Read directory.
 */
int tmpfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct tmpfs_dir_entry de;

	/* walk through all entries */
	for (;;) {
		/* read next drectory entry */
		if (tmpfs_file_read(filp, (char *) &de, sizeof(struct tmpfs_dir_entry), &filp->f_pos) != sizeof(struct tmpfs_dir_entry))
			return 0;

		/* skip null entries */
		if (!de.d_inode)
			continue;

		/* fill in directory entry */
		if (filldir(dirent, de.d_name, strlen(de.d_name), filp->f_pos, de.d_inode)) {
			filp->f_pos -= sizeof(struct tmpfs_dir_entry);
			return 0;
		}
	}

	return 0;
}
