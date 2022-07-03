#include <fs/fs.h>
#include <string.h>
#include <stdio.h>

/*
 * Read file systems.
 */
static int proc_filesystems_read(struct file_t *filp, char *buf, int count)
{
	char tmp_buf[256];
	size_t len;

	/* get file systems list */
	len = get_filesystem_list(tmp_buf);

	/* file position after end */
	if (filp->f_pos >= len)
		return 0;

	/* update count */
	if (filp->f_pos + count > len)
		count = len - filp->f_pos;

	/* copy content to user buffer and update file position */
	memcpy(buf, tmp_buf + filp->f_pos, count);
	filp->f_pos += count;

	return count;
}

/*
 * Filesystems file operations.
 */
struct file_operations_t proc_filesystems_fops = {
	.read		= proc_filesystems_read,
};

/*
 * Filesystems inode operations.
 */
struct inode_operations_t proc_filesystems_iops = {
	.fops		= &proc_filesystems_fops,
};

