#include <fs/dev_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Directory lookup.
 */
int devfs_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode)
{
	struct devfs_entry_t *de_dir, *de;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* devfs entry must be a directory */
	de_dir = dir->u.generic_i;
	if (!S_ISDIR(de_dir->mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* find entry */
	de = devfs_find(de_dir, name, name_len);
	if (!de) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode */
	*res_inode = devfs_iget(dir->i_sb, de);
	if (!*res_inode) {
		iput(dir);
		return -EACCES;
	}

	iput(dir);
	return 0;
}