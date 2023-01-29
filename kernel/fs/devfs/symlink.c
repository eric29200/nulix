#include <fs/dev_fs.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Resolve a symbolic link.
 */
int devfs_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode)
{
	struct devfs_entry_t *de;

	*res_inode = NULL;

	/* check inode */
	if (!inode)
		return -ENOENT;

	/* not a link */
	if (!S_ISLNK(inode->i_mode)) {
		*res_inode = inode;
		return 0;
	}

	/* get devfs entry */
	de = (struct devfs_entry_t *) inode->u.generic_i;

	/* release link inode */
	iput(inode);

	/* open target inode */
	return open_namei(AT_FDCWD, dir, de->u.symlink.linkname, flags, mode, res_inode);
}

/*
 * Read value of a symbolic link.
 */
ssize_t devfs_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
	struct devfs_entry_t *de;

	/* inode must be link */
	if (!S_ISLNK(inode->i_mode)) {
		iput(inode);
		return -EINVAL;
	}

	/* get devfs entry */
	de = (struct devfs_entry_t *) inode->u.generic_i;

	/* limit buffer size to page size */
	if (bufsize > de->u.symlink.linkname_len)
		bufsize = de->u.symlink.linkname_len;

	/* release inode */
	iput(inode);

	/* copy target name to user buffer */
	memcpy(buf, de->u.symlink.linkname, bufsize);

	return bufsize;
}
