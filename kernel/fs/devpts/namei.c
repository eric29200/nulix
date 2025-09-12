#include <fs/devpts_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Directory lookup.
 */
struct dentry *devpts_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct devpts_entry *de;

	/* find entry */
	de = devpts_find(dir->u.generic_i, dentry->d_name.name, dentry->d_name.len);
	if (!de)
		goto out;

	/* get inode */
	inode = devpts_iget(dir->i_sb, de);
	if (!inode)
		return ERR_PTR(-EACCES);

out:
	d_add(dentry, inode);
	return NULL;
}