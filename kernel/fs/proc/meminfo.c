#include <fs/proc_fs.h>
#include <mm/paging.h>
#include <stdio.h>
#include <string.h>

/*
 * Read meminfo.
 */
static int proc_meminfo_read(struct file *filp, char *buf, int count)
{
	uint32_t nr_free_pages = 0, i;
	char tmp_buf[256];
	size_t len;

	/* get number of free pages */
	for (i = 0; i < nr_pages; i++)
		if (!page_array[i].count)
			nr_free_pages++;

	/* print meminfo */
	len = sprintf(tmp_buf,
		"MemTotal:\t%d kB\n"
		"MemFree:\t%d kB\n"
		"Buffers:\t%d kB\n",
		nr_pages * PAGE_SIZE / 1024,
		nr_free_pages * PAGE_SIZE / 1024,
		buffermem / 1024);

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
struct file_operations proc_meminfo_fops = {
	.read		= proc_meminfo_read,
};

/*
 * Meminfo inode operations.
 */
struct inode_operations proc_meminfo_iops = {
	.fops		= &proc_meminfo_fops,
};

