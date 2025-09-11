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
	int ret;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* devpts entry must be a directory */
	de_dir = dir->u.generic_i;
	ret = -ENOTDIR;
	if (!S_ISDIR(de_dir->mode))
		goto out;

	/* find entry */
	ret = -ENOENT;
	de = devpts_find(de_dir, name, name_len);
	if (!de)
		goto out;

	/* get inode */
	*res_inode = devpts_iget(dir->i_sb, de);
	if (!*res_inode)
		ret = -EACCES;
	else
		ret = 0;

out:
	iput(dir);
	return ret;
}