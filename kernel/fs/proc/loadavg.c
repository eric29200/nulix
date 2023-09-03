#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

#define LOAD_INT(x)		((x) >> FSHIFT)
#define LOAD_FRAC(x)		LOAD_INT(((x) & (FIXED_1 - 1)) * 100)

/*
 * Read load average.
 */
static int proc_loadavg_read(struct file *filp, char *buf, int count)
{
	int ntasks = 0, nrun = 0, a, b, c;
	struct list_head *pos;
	struct task *task;
	char tmp_buf[256];
	size_t len;

	/* get number of tasks */
	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);
		ntasks++;

		if (task->state == TASK_RUNNING)
			nrun++;
	}

	/* compute load average */
	CALC_LOAD(avenrun[0], EXP_1, nrun);
	CALC_LOAD(avenrun[1], EXP_5, nrun);
	CALC_LOAD(avenrun[2], EXP_15, nrun);
	a = avenrun[0] + (FIXED_1 / 200);
	b = avenrun[0] + (FIXED_1 / 200);
	c = avenrun[0] + (FIXED_1 / 200);

	/* print load average in buffer */
	len = sprintf(tmp_buf, "%d.%d %d.%d %d.%d %d/%d %d\n",
		      LOAD_INT(a), LOAD_FRAC(a),
		      LOAD_INT(b), LOAD_FRAC(b),
		      LOAD_INT(c), LOAD_FRAC(c),
		      nrun, ntasks, last_pid);

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
 * Load average file operations.
 */
struct file_operations proc_loadavg_fops = {
	.read		= proc_loadavg_read,
};

/*
 * Load average inode operations.
 */
struct inode_operations proc_loadavg_iops = {
	.fops		= &proc_loadavg_fops,
};

