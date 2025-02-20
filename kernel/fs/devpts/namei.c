#include <fs/devpts_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Directory lookup.
 */
int devpts_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode)
{
	struct devpts_entry *de_dir, *de;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* devpts entry must be a directory */
	de_dir = dir->u.generic_i;
	if (!S_ISDIR(de_dir->mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* find entry */
	de = devpts_find(de_dir, name, name_len);
	if (!de) {
		iput(dir);
		return -ENOENT;
	}

	/* get inode */
	*res_inode = devpts_iget(dir->i_sb, de);
	if (!*res_inode) {
		iput(dir);
		return -EACCES;
	}

	iput(dir);
	return 0;
}