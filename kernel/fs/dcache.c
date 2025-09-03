#include <fs/fs.h>

/*
 * Release a dentry.
 */
void dput(struct dentry *dentry)
{
	if (!dentry)
		return;

	/* release inode */
	iput(dentry->d_inode);
}

/*
 * Instantiate a dentry.
 */
void d_instantiate(struct dentry *dentry, struct inode *inode)
{
	dentry->d_inode = inode;
}