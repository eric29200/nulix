#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Resolve a symbolic link.
 */
struct dentry *minix_follow_link(struct inode *inode, struct dentry *base)
{
	struct buffer_head *bh;

	/* get first link block */
	bh = minix_bread(inode, 0, 0);
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
ssize_t minix_readlink(struct inode *inode, char *buf, size_t bufsize)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh;
	size_t len;

	/* inode must be link */
	if (!S_ISLNK(inode->i_mode))
		return -EINVAL;

	/* limit buffer size to block size */
	if (bufsize > sb->s_blocksize - 1)
		bufsize = sb->s_blocksize - 1;

	/* check 1st block */
	if (!inode->u.minix_i.i_zone[0])
		return 0;

	/* get 1st block */
	bh = minix_bread(inode, 0, 0);
	if (!bh)
		return 0;

	/* copy target name to user buffer */
	for (len = 0; len < bufsize; len++)
		buf[len] = bh->b_data[len];

	/* release buffer */
	brelse(bh);
	return len;
}
