#include <fs/proc_fs.h>
#include <sys/sys.h>
#include <mm/paging.h>
#include <stdio.h>
#include <string.h>

/*
 * Read meminfo.
 */
static int proc_meminfo_read(struct file *filp, char *buf, int count)
{
	struct sysinfo info;
	char tmp_buf[256];
	size_t len;

	/* get informations */
	sys_sysinfo(&info);

	/* print meminfo */
	len = sprintf(tmp_buf,
		"MemTotal:  %d kB\n"
		"MemFree:   %d kB\n"
		"Buffers:   %d kB\n"
		"Cached:    %d kB\n",
		info.totalram >> 10,
		info.freeram >> 10,
		info.bufferram >> 10,
		page_cache_size >> (PAGE_SHIFT - 10));

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

