#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <kernel_stat.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Read stat.
 */
static int proc_kstat_read(struct file_t *filp, char *buf, int count)
{
	char tmp_buf[256];
	size_t len;

	/* print boot time */
	len = sprintf(tmp_buf,	"cpu 0 0 0 0 0 0 0 0 0 0\n"
				"intr %u\n"
				"ctxt %u\n"
				"btime %u\n",
		      kstat.interrupts,
		      kstat.context_switch,
		      startup_time);

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
 * Stat file operations.
 */
struct file_operations_t proc_kstat_fops = {
	.read		= proc_kstat_read,
};

/*
 * Stat inode operations.
 */
struct inode_operations_t proc_kstat_iops = {
	.fops		= &proc_kstat_fops,
};
