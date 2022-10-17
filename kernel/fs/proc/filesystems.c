#include <fs/fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Read file systems.
 */
static int proc_filesystems_read(struct file_t *filp, char *buf, int count)
{
	char *tmp_buf;
	size_t len;

	/* allocate temp buffer */
	tmp_buf = (char *) get_free_page();
	if (!tmp_buf)
		return -ENOMEM;

	/* get file systems list */
	len = get_filesystem_list(tmp_buf, PAGE_SIZE);

	/* file position after end */
	if (filp->f_pos >= len) {
		count = 0;
		goto out;
	}

	/* update count */
	if (filp->f_pos + count > len)
		count = len - filp->f_pos;

	/* copy content to user buffer and update file position */
	memcpy(buf, tmp_buf + filp->f_pos, count);
	filp->f_pos += count;

out:
	free_page(tmp_buf);
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

