#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Follow a link (inode will be released).
 */
int ext2_fast_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode)
{
	char *target;
	int ret;

	/* reset result inode */
	*res_inode = NULL;

	if (!inode)
		return -ENOENT;

	/* check if a inode is a link */
	if (!S_ISLNK(inode->i_mode)) {
		*res_inode = inode;
		return 0;
	}

	/* get target link */
	target = (char *) inode->u.ext2_i.i_data;

	/* open target inode */
	ret = open_namei(AT_FDCWD, dir, target, flags, mode, res_inode);

	/* release inode */
	iput(inode);

	return ret;
}

/*
 * Read value of a symbolic link.
 */
ssize_t ext2_fast_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
	char *target;
	size_t len;

	/* inode must be a link */
	if (!S_ISLNK(inode->i_mode)) {
		iput(inode);
		return -EINVAL;
	}

	/* get target link */
	target = (char *) inode->u.ext2_i.i_data;

	/* limit length */
	len = strlen(target);
	if (len > bufsize)
		len = bufsize;

	/* copy target */
	memcpy(buf, target, len);

	/* release inode */
	iput(inode);

	return len;
}

/*
 * Resolve a symbolic link.
 */
int ext2_page_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode)
{
	struct buffer_head_t *bh;
	int ret;

	*res_inode = NULL;

	/* null inode */
	if (!inode) {
		return -ENOENT;
	}

	if (!S_ISLNK(inode->i_mode)) {
		*res_inode = inode;
		return 0;
	}

	/* read first link block */
	bh = ext2_bread(inode, 0, 0);
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
ssize_t ext2_page_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
	struct buffer_head_t *bh;
	size_t len;

	/* inode must be link */
	if (!S_ISLNK(inode->i_mode)) {
		iput(inode);
		return -EINVAL;
	}

	/* limit buffer size to block size */
	if (bufsize > inode->i_sb->s_blocksize)
		bufsize = inode->i_sb->s_blocksize - 1;

	/* check 1st block */
	if (!inode->u.ext2_i.i_data[0]) {
		iput(inode);
		return 0;
	}

	/* read 1st block */
	bh = ext2_bread(inode, 0, 0);
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