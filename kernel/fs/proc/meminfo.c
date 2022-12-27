#include <fs/proc_fs.h>
#include <mm/paging.h>
#include <stdio.h>
#include <string.h>

/*
 * Read meminfo.
 */
static int proc_meminfo_read(struct file_t *filp, char *buf, int count)
{
	char tmp_buf[256];
	size_t len;

	/* print meminfo */
	len = sprintf(tmp_buf, "MemTotal:\t%d kB\n", nr_pages * PAGE_SIZE / 1024);

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
 * Meminfo file operations.
 */
struct file_operations_t proc_meminfo_fops = {
	.read		= proc_meminfo_read,
};

/*
 * Meminfo inode operations.
 */
struct inode_operations_t proc_meminfo_iops = {
	.fops		= &proc_meminfo_fops,
};

