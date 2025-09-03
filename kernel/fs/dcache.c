#include <fs/fs.h>
#include <stdio.h>

/*
 * Release a dentry.
 */
void dput(struct dentry *dentry)
{
	if (!dentry)
		return;

	/* update reference count */
	dentry->d_count--;
	if (dentry->d_count < 0)
		panic("dput: negative d_count");

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

/*
 * Delete a dentry.
 */
void d_delete(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;

	dentry->d_inode = NULL;
	iput(inode);
}