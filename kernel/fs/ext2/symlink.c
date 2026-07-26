#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <stderr.h>
#include <fcntl.h>
#include <stdio.h>

/*
 * Follow a link.
 */
struct dentry *ext2_fast_follow_link(struct dentry *dentry, struct dentry *base)
{
	struct inode *inode = dentry->d_inode;
	char *target;

	/* get target link */
	target = (char *) inode->u.ext2_i.i_data;

	/* resolve target */
	return lookup_dentry(AT_FDCWD, base, target, 1);
}

/*
 * Read value of a symbolic link.
 */
ssize_t ext2_fast_readlink(struct dentry *dentry, char *buf, size_t bufsize)
{
	struct inode *inode = dentry->d_inode;
	char *target;
	size_t len;

	/* inode must be a link */
	if (!S_ISLNK(inode->i_mode))
		return -EINVAL;

	/* get target link */
	target = (char *) inode->u.ext2_i.i_data;

	/* limit length */
	len = strlen(target);
	if (len > bufsize)
		len = bufsize;

	/* copy target */
	memcpy(buf, target, len);

	return len;
}