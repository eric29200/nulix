#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <stderr.h>
#include <fcntl.h>
#include <stdio.h>

/*
 * Follow a link.
 */
struct dentry *ext2_fast_follow_link(struct inode *inode, struct dentry *base)
{
	char *target;

	/* get target link */
	target = (char *) inode->u.ext2_i.i_data;

	/* resolve target */
	return lookup_dentry(AT_FDCWD, base->d_inode, target, 1);
}

/*
 * Read value of a symbolic link.
 */
ssize_t ext2_fast_readlink(struct inode *inode, char *buf, size_t bufsize)
{
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

/*
 * Resolve a symbolic link.
 */
struct dentry *ext2_page_follow_link(struct inode *inode, struct dentry *base)
{
	struct buffer_head *bh;

	/* read first link block */
	bh = ext2_bread(inode, 0, 0);
	if (!bh) {
		dput(base);
		return ERR_PTR(-EIO);
	}

	/* resolve target */
	base = lookup_dentry(AT_FDCWD, base->d_inode, bh->b_data, 1);

	brelse(bh);
	return base;
}

/*
 * Read value of a symbolic link.
 */
ssize_t ext2_page_readlink(struct inode *inode, char *buf, size_t bufsize)
{
	struct buffer_head *bh;
	size_t len;

	/* inode must be link */
	if (!S_ISLNK(inode->i_mode))
		return -EINVAL;

	/* limit buffer size to block size */
	if (bufsize > inode->i_sb->s_blocksize)
		bufsize = inode->i_sb->s_blocksize - 1;

	/* check 1st block */
	if (!inode->u.ext2_i.i_data[0])
		return 0;

	/* read 1st block */
	bh = ext2_bread(inode, 0, 0);
	if (!bh)
		return 0;

	/* copy target name to user buffer */
	for (len = 0; len < bufsize; len++)
		buf[len] = bh->b_data[len];

	/* release buffer */
	brelse(bh);
	return len;
}
