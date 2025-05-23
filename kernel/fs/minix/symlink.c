#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Resolve a symbolic link.
 */
int minix_follow_link(struct inode *dir, struct inode *inode, int flags, mode_t mode, struct inode **res_inode)
{
	struct buffer_head *bh;
	int ret;

	*res_inode = NULL;

	/* null inode */
	if (!inode)
		return -ENOENT;

	/* not a link */
	if (!S_ISLNK(inode->i_mode)) {
		*res_inode = inode;
		return 0;
	}

	/* get first link block */
	bh = minix_bread(inode, 0, 0);
	if (!bh) {
		iput(inode);
		return -EIO;
	}

	/* release link inode */
	iput(inode);

	/* open target inode */
	ret = open_namei(AT_FDCWD, dir, bh->b_data, flags, mode, res_inode);

	/* release block buffer */
	brelse(bh);

	return ret;
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
	if (!S_ISLNK(inode->i_mode)) {
		iput(inode);
		return -EINVAL;
	}

	/* limit buffer size to block size */
	if (bufsize > sb->s_blocksize - 1)
		bufsize = sb->s_blocksize - 1;

	/* check 1st block */
	if (!inode->u.minix_i.i_zone[0]) {
		iput(inode);
		return 0;
	}

	/* get 1st block */
	bh = minix_bread(inode, 0, 0);
	if (!bh) {
		iput(inode);
		return 0;
	}

	/* release inode */
	iput(inode);

	/* copy target name to user buffer */
	for (len = 0; len < bufsize; len++)
		buf[len] = bh->b_data[len];

	/* release buffer */
	brelse(bh);
	return len;
}
