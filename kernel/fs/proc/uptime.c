#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Read uptime.
 */
static int proc_uptime_read(struct file *filp, char *buf, int count)
{
	struct task *init_task;
	char tmp_buf[256];
	size_t len;

	/* get init task */
	init_task = list_first_entry(&tasks_list, struct task, list);

	/* print uptime in temporary buffer */
	len = sprintf(tmp_buf, "%d.%d %d.%d\n", jiffies / HZ, jiffies % HZ, init_task->utime / HZ, init_task->utime % HZ);

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
 * Uptime file operations.
 */
struct file_operations proc_uptime_fops = {
	.read		= proc_uptime_read,
};

/*
 * Uptime inode operations.
 */
struct inode_operations proc_uptime_iops = {
	.fops		= &proc_uptime_fops,
};

