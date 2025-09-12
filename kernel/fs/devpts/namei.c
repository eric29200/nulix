#include <fs/devpts_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Directory lookup.
 */
int devpts_lookup(struct inode *dir, struct dentry *dentry)
{
	struct devpts_entry *de_dir, *de;
	struct inode *inode = NULL;

	/* check directory */
	if (!dir)
		return -ENOENT;

	/* devpts entry must be a directory */
	de_dir = dir->u.generic_i;
	if (!S_ISDIR(de_dir->mode))
		return -ENOENT;

	/* find entry */
	de = devpts_find(de_dir, dentry->d_name.name, dentry->d_name.len);
	if (!de)
		goto out;

	/* get inode */
	inode = devpts_iget(dir->i_sb, de);
	if (!inode)
		return -EACCES;

out:
	d_add(dentry, inode);
	return 0;
}