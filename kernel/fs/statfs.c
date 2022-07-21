#include <fs/fs.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Get statistics on file system.
 */
int do_statfs64(struct inode_t *inode, struct statfs64_t *buf)
{
	/* check if statfs is implemented */
	if (!inode || !inode->i_sb || !inode->i_sb->s_op || !inode->i_sb->s_op->statfs)
		return -ENOSYS;

	/* do statfs */
	inode->i_sb->s_op->statfs(inode->i_sb, buf);

	return 0;
}
