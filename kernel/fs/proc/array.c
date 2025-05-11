#include <fs/fs.h>
#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <sys/sys.h>
#include <sys/syscall.h>
#include <kernel_stat.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

#define LOAD_INT(x)		((x) >> FSHIFT)
#define LOAD_FRAC(x)		LOAD_INT(((x) & (FIXED_1 - 1)) * 100)

/*
 * Read file systems.
 */
static int proc_filesystems_read(char *page)
{
	return get_filesystem_list(page, PAGE_SIZE);
}

/*
 * Read load average.
 */
static int proc_loadavg_read(char *page)
{
	int ntasks = 0, nrun = 0, a, b, c;
	struct list_head *pos;
	struct task *task;

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
	return sprintf(page, "%d.%d %d.%d %d.%d %d/%d %d\n",
		      LOAD_INT(a), LOAD_FRAC(a),
		      LOAD_INT(b), LOAD_FRAC(b),
		      LOAD_INT(c), LOAD_FRAC(c),
		      nrun, ntasks, last_pid);
}

/*
 * Read meminfo.
 */
static int proc_meminfo_read(char *page)
{
	struct sysinfo info;

	/* get informations */
	sys_sysinfo(&info);

	/* print meminfo */
	return sprintf(page,
		"MemTotal:  %d kB\n"
		"MemFree:   %d kB\n"
		"MemShared: %d kB\n"
		"Buffers:   %d kB\n"
		"Cached:    %d kB\n",
		info.totalram >> 10,
		info.freeram >> 10,
		info.sharedram >> 10,
		info.bufferram >> 10,
		page_cache_size << (PAGE_SHIFT - 10));
}

/*
 * Read file mounts.
 */
static int proc_mounts_read(char *page)
{
	return get_vfs_mount_list(page, PAGE_SIZE);
}

/*
 * Read stat.
 */
static int proc_kstat_read(char *page)
{
	/* print boot time */
	return sprintf(page,	"cpu 0 0 0 0 0 0 0 0 0 0\n"
				"intr %u\n"
				"ctxt %u\n"
				"btime %llu\n",
		      kstat.interrupts,
		      kstat.context_switch,
		      startup_time);
}

/*
 * Read uptime.
 */
static int proc_uptime_read(char *page)
{
	struct task *init_task;

	/* get init task */
	init_task = list_first_entry(&tasks_list, struct task, list);

	/* print uptime in temporary buffer */
	return sprintf(page, "%lld.%lld %lld.%lld\n", jiffies / HZ, jiffies % HZ, init_task->utime / HZ, init_task->utime % HZ);
}

/*
 * Read array.
 */
static int proc_array_read(struct file *filp, char *buf, int count)
{
	char *page;
	size_t len;

	/* get a free page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* get informations */
	switch (filp->f_inode->i_ino) {
		case PROC_UPTIME_INO:
			len = proc_uptime_read(page);
			break;
		case PROC_FILESYSTEMS_INO:
			len = proc_filesystems_read(page);
			break;
		case PROC_MOUNTS_INO:
			len = proc_mounts_read(page);
			break;
		case PROC_KSTAT_INO:
			len = proc_kstat_read(page);
			break;
		case PROC_MEMINFO_INO:
			len = proc_meminfo_read(page);
			break;
		case PROC_LOADAVG_INO:
			len = proc_loadavg_read(page);
			break;
		default:
			count = -ENOMEM;
			goto out;
	}

	/* file position after end */
	if (filp->f_pos >= len) {
		count = 0;
		goto out;
	}

	/* update count */
	if (filp->f_pos + count > len)
		count = len - filp->f_pos;

	/* copy content to user buffer and update file position */
	memcpy(buf, page + filp->f_pos, count);
	filp->f_pos += count;

out:
	free_page(page);
	return count;
}

/*
 * Array file operations.
 */
struct file_operations proc_array_fops = {
	.read		= proc_array_read,
};

/*
 * Array inode operations.
 */
struct inode_operations proc_array_iops = {
	.fops		= &proc_array_fops,
};