#include <fs/fs.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Get statistics on file system.
 */
int do_statfs64(const char *path, struct statfs64_t *buf)
{
	struct inode_t *inode;

	/* get inode */
	inode = namei(AT_FDCWD, NULL, path, 1);
	if (!inode)
		return -ENOENT;

	/* check if statfs is implemented */
	if (!inode->i_sb || !inode->i_sb->s_op || !inode->i_sb->s_op->statfs)
		return -ENOSYS;

	/* do statfs */
	inode->i_sb->s_op->statfs(inode->i_sb, buf);

	/* release inode */
	iput(inode);

	return 0;
}
