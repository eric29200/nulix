#include <fs/fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Read file mounts.
 */
static int proc_mounts_read(struct file_t *filp, char *buf, int count)
{
	char *tmp_buf;
	size_t len;

	/* allocate temp buffer */
	tmp_buf = (char *) get_free_kernel_page();
	if (!tmp_buf)
		return -ENOMEM;

	/* get mounted file systems list */
	len = get_vfs_mount_list(tmp_buf, PAGE_SIZE);

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
	free_kernel_page(tmp_buf);
	return count;
}

/*
 * Mounts file operations.
 */
struct file_operations_t proc_mounts_fops = {
	.read		= proc_mounts_read,
};

/*
 * Mounts inode operations.
 */
struct inode_operations_t proc_mounts_iops = {
	.fops		= &proc_mounts_fops,
};

