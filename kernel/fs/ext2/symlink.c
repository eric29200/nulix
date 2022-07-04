#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Follow a link (inode will be released).
 */
int ext2_follow_link(struct inode_t *dir, struct inode_t *inode, struct inode_t **res_inode)
{
	char *target;

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

	/* resolve target inode */
	*res_inode = namei(AT_FDCWD, dir, target, 0);
	if (!*res_inode)
		return -EACCES;

	/* release inode */
	iput(inode);

	/* release block buffer */
	return 0;
}

/*
 * Read value of a symbolic link.
 */
ssize_t ext2_readlink(struct inode_t *inode, char *buf, size_t bufsize)
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
