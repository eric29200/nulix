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
 * Calculate metrics.
 */
static int proc_calc_metrics(char *page, char **start, off_t off, size_t count, int *eof, size_t len)
{
	/* end of file ? */
	if (off + count >= len)
		*eof = 1;

	/* set start buffer */
	*start = page + off;

	/* check offset */
	if (off >= len)
		return 0;

	/* compute length */
	len -= off;
	if (len > count)
		len = count;

	return len;
}

/*
 * Read file systems.
 */
static int filesystems_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	size_t len;

	/* get filesystems list */
	len = get_filesystem_list(page, PAGE_SIZE);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

/*
 * Read load average.
 */
static int loadavg_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	int nrun = 0, a, b, c;
	size_t len;

	/* compute load average */
	CALC_LOAD(avenrun[0], EXP_1, nrun);
	CALC_LOAD(avenrun[1], EXP_5, nrun);
	CALC_LOAD(avenrun[2], EXP_15, nrun);
	a = avenrun[0] + (FIXED_1 / 200);
	b = avenrun[0] + (FIXED_1 / 200);
	c = avenrun[0] + (FIXED_1 / 200);

	/* print load average in buffer */
	len = sprintf(page, "%d.%d %d.%d %d.%d %d/%d %d\n",
		      LOAD_INT(a), LOAD_FRAC(a),
		      LOAD_INT(b), LOAD_FRAC(b),
		      LOAD_INT(c), LOAD_FRAC(c),
		      nrun, nr_tasks, last_pid);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

/*
 * Read meminfo.
 */
static int meminfo_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	struct sysinfo info;
	size_t len;

	/* get informations */
	sys_sysinfo(&info);

	/* print meminfo */
	len = sprintf(page,
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

	return proc_calc_metrics(page, start, off, count, eof, len);
}

/*
 * Read file mounts.
 */
static int mounts_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	size_t len;

	/* get mounts list */
	len = get_vfs_mount_list(page, PAGE_SIZE);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

/*
 * Read stat.
 */
static int kstat_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	uint32_t intr = 0;
	size_t len, i;

	/* compute number of interrupts */
	for (i = 0; i < NR_IRQS; i++)
		intr += kstat.irqs[i];

	/* print boot time */
	len = sprintf(page,	"cpu 0 0 0 0 0 0 0 0 0 0\n"
				"intr %u\n"
				"ctxt %u\n"
				"btime %llu\n",
		      intr,
		      kstat.context_switch,
		      startup_time);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

/*
 * Read uptime.
 */
static int uptime_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	struct task *init_task;
	size_t len;

	/* get init task */
	init_task = list_first_entry(&tasks_list, struct task, list);

	/* print uptime in temporary buffer */
	len = sprintf(page, "%ld.%ld %ld.%ld\n", jiffies / HZ, jiffies % HZ, init_task->utime / HZ, init_task->utime % HZ);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

/*
 * Read kernel command line.
 */
static int kcmdline_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	extern char saved_command_line[];
	size_t len;

	/* print command line in temporary buffer */
	len = sprintf(page, "%s\n", saved_command_line);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

/*
 * Read devices.
 */
static int devices_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	size_t len;

	/* get device list */
	len = get_device_list(page);

	return proc_calc_metrics(page, start, off, count, eof, len);
}

/*
 * Init misc proc entries.
 */
void proc_misc_init()
{
	create_proc_read_entry("uptime", 0, NULL, uptime_read_proc);
	create_proc_read_entry("filesystems", 0, NULL, filesystems_read_proc);
	create_proc_read_entry("mounts", 0, NULL, mounts_read_proc);
	create_proc_read_entry("stat", 0, NULL, kstat_read_proc);
	create_proc_read_entry("meminfo", 0, NULL, meminfo_read_proc);
	create_proc_read_entry("loadavg", 0, NULL, loadavg_read_proc);
	create_proc_read_entry("cmdline", 0, NULL, kcmdline_read_proc);
	create_proc_read_entry("devices", 0, NULL, devices_read_proc);
}