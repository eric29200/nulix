#include <drivers/char/mem.h>
#include <drivers/char/keyboard.h>
#include <proc/sched.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <x86/io.h>
#include <stdio.h>
#include <stderr.h>
#include <resource.h>
#include <reboot.h>
#include <prctl.h>
 
#define UTS_SYSNAME		"nulix"
#define UTS_NODENAME		"nulix"
#define UTS_RELEASE		"2.4.0"
#define UTS_VERSION		"nulix 2.4.0"
#define UTS_MACHINE		"x86"

/*
 * Utsname.
 */
struct utsname system_utsname = {
	UTS_SYSNAME,
	UTS_NODENAME,
	UTS_RELEASE,
	UTS_VERSION,
	UTS_MACHINE,
};

/*
 * Get time system call.
 */
int sys_clock_gettime64(clockid_t clockid, struct timespec *tp)
{
	switch (clockid) {
		case CLOCK_REALTIME:
			tp->tv_sec = startup_time + xtimes.tv_sec;
			tp->tv_nsec = xtimes.tv_nsec;
			break;
		case CLOCK_MONOTONIC:
		case CLOCK_MONOTONIC_RAW:
			tp->tv_sec = xtimes.tv_sec;
			tp->tv_nsec = xtimes.tv_nsec;
			break;
		case CLOCK_BOOTTIME:
			tp->tv_sec = xtimes.tv_sec;
			tp->tv_nsec = xtimes.tv_nsec;
			break;
		default:
			printf("clock_gettime64 not implement on clockid=%d\n", clockid);
			return -ENOSYS;
	}

	return 0;
}

/*
 * Get time system call.
 */
int sys_clock_gettime32(clockid_t clockid, struct old_timespec *tp)
{
	switch (clockid) {
		case CLOCK_REALTIME:
			tp->tv_sec = startup_time + xtimes.tv_sec;
			tp->tv_nsec = xtimes.tv_nsec;
			break;
		case CLOCK_MONOTONIC:
			tp->tv_sec = xtimes.tv_sec;
			tp->tv_nsec = xtimes.tv_nsec;
			break;
		case CLOCK_BOOTTIME:
			tp->tv_sec = xtimes.tv_sec;
			tp->tv_nsec = xtimes.tv_nsec;
			break;
		default:
			printf("clock_gettime32 not implement on clockid=%d\n", clockid);
			return -ENOSYS;
	}

	return 0;
}

/*
 * Get time in seconds.
 */
int sys_time(int *tloc)
{
	time_t ret = xtimes.tv_sec;

	if (tloc)
		*tloc = ret;

	return ret;
}

/*
 * Get random system call.
 */
int sys_getrandom(void *buf, size_t buflen, unsigned int flags)
{
	/* unused flags */
	UNUSED(flags);

	/* use /dev/random */
	return random_read(NULL, buf, buflen, NULL);
}

/*
 * Get rusage system call.
 */
int sys_getrusage(int who, struct rusage *ru)
{
	if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN)
		return -EINVAL;

	/* reset rusage */
	memset(ru, 0, sizeof(struct rusage));

	return 0;
}

/*
 * Pause system call.
 */
int sys_pause()
{
	/* set current state sleeping and reschedule */
	current_task->state = TASK_SLEEPING;
	schedule();

	return -ERESTARTNOHAND;
}

/*
 * Prlimit system call.
 */
int sys_prlimit64(pid_t pid, int resource, struct rlimit64 *new_limit, struct rlimit64 *old_limit)
{
	struct task *task;

	/* check resource */
	if (resource >= RLIM_NLIMITS)
		return -EINVAL;

	/* get task */
	if (pid)
		task = find_task(pid);
	else
		task = current_task;

	/* no matching task */
	if (!task)
		return -ESRCH;

	/* get old limit */
	if (old_limit) {
		memset(old_limit, 0, sizeof(struct rlimit64));
		old_limit->rlim_cur = task->rlim[resource].rlim_cur;
		old_limit->rlim_max = task->rlim[resource].rlim_max;
	}

	/* write new limit */
	if (new_limit) {
		task->rlim[resource].rlim_cur = new_limit->rlim_cur;
		task->rlim[resource].rlim_max = new_limit->rlim_max;
	}

	return 0;
}

/*
 * Restart CPU.
 */
static int do_restart()
{
	uint8_t out;

	/* disable interrupts */
	irq_disable();

	/* clear keyboard buffer */
	out = 0x02;
	while (out & 0x02)
		out = inb(KEYBOARD_STATUS);

	/* pluse CPU reset line */
	outb(KEYBOARD_STATUS, KEYBOARD_RESET);
	halt();

	return 0;
}

/*
 * Reboot system call.
 */
int sys_reboot(int magic1, int magic2, int cmd, void *arg)
{
	/* unused argument */
	UNUSED(arg);

	/* check magic */
	if ((uint32_t) magic1 != LINUX_REBOOT_MAGIC1 || (
		magic2 != LINUX_REBOOT_MAGIC2
		&& magic2 != LINUX_REBOOT_MAGIC2A
		&& magic2 != LINUX_REBOOT_MAGIC2B
		&& magic2 != LINUX_REBOOT_MAGIC2C))
		return -EINVAL;

	switch (cmd) {
		case LINUX_REBOOT_CMD_RESTART:
		case LINUX_REBOOT_CMD_RESTART2:
		case LINUX_REBOOT_CMD_POWER_OFF:
		case LINUX_REBOOT_CMD_HALT:
			return do_restart();
		case LINUX_REBOOT_CMD_CAD_ON:
			break;
		case LINUX_REBOOT_CMD_CAD_OFF:
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

/*
 * System info system call.
 */
int sys_sysinfo(struct sysinfo *info)
{
	/* clear info */
	memset(info, 0, sizeof(struct sysinfo));

	/* set info */
	info->uptime = jiffies / HZ;
	info->procs = nr_tasks - 1;
	info->totalram = totalram_pages << PAGE_SHIFT;
	info->freeram = nr_free_pages() << PAGE_SHIFT;
	info->bufferram = buffermem_pages << PAGE_SHIFT;

	return 0;
}

/*
 * Umask system call.
 */
mode_t sys_umask(mode_t mask)
{
	mode_t ret = current_task->fs->umask;
	current_task->fs->umask = mask & 0777;
	return ret;
}

/*
 * Uname system call.
 */
int sys_uname(struct utsname *buf)
{
	if (!buf)
		return -EINVAL;

	memcpy(buf->sysname, system_utsname.sysname, sizeof(system_utsname.sysname));
	memcpy(buf->nodename, system_utsname.nodename, sizeof(system_utsname.nodename));
	memcpy(buf->release, system_utsname.release, sizeof(system_utsname.release));
	memcpy(buf->version, system_utsname.version, sizeof(system_utsname.version));
	memcpy(buf->machine, system_utsname.machine, sizeof(system_utsname.machine));

	return 0;
}

/*
 * Set host name system call.
 */
int sys_sethostname(const char *name, size_t len)
{
	/* check length */
	if (len + 1 > UTSNAME_LEN)
		return -EINVAL;

	/* set hostname */
	memcpy(system_utsname.nodename, name, len);
	system_utsname.nodename[len] = 0;

	return 0;
}

/*
 * Select a process (for get/set/priority). Returns true if task matches which and who.
 */
static int proc_sel(struct task *task, int which, int who)
{
	if (!task->pid)
		return 0;

	switch (which) {
		case PRIO_PROCESS:
			if (!who && task == current_task)
				return 1;
			return task->pid == who;
		case PRIO_PGRP:
			if (!who)
				who = current_task->pgrp;
			return task->pgrp == who;
		case PRIO_USER:
			if (!who)
				who = current_task->uid;
			return (int) task->uid == who;
	}

	return 0;
}

/*
 * Get priority system call.
 */
int sys_getpriority(int which, int who)
{
	int max_prio = -ESRCH;
	struct list_head *pos;
	struct task *task;

	/* check process selection */
	if (which < PRIO_PROCESS || which > PRIO_PROCESS)
		return -EINVAL;

	/* for each task */
	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);

		/* check if task matches which and who */
		if (!proc_sel(task, which, who))
			continue;

		/* return max priority */
		if (task->priority > max_prio)
			max_prio = task->priority;
	}

	/* scale the priority from timeslice to 0..40 */
	if (max_prio > 0)
		max_prio = (max_prio * 20 + DEF_PRIORITY / 2) / DEF_PRIORITY;

	return max_prio;
}

/*
 * Set priority system call.
 */
int sys_setpriority(int which, int who, int niceval)
{
	struct list_head *pos;
	struct task *task;
	int ret = -ESRCH;
	int priority;

	/* check process selection */
	if (which < PRIO_PROCESS || which > PRIO_PROCESS)
		return -EINVAL;

	/* normalize : avoid signed division */
	priority = niceval;
	if (niceval < 0)
		priority = -niceval;
	if (priority > 20)
		priority = 20;

	priority = (priority * DEF_PRIORITY + 10) / 20 + DEF_PRIORITY;

	if (niceval >= 0) {
		priority = 2 * DEF_PRIORITY - priority;
		if (!priority)
			priority = 1;
	}

	/* for each task */
	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);

		/* check if task matches which and who */
		if (!proc_sel(task, which, who))
			continue;

		/* set priority */
		task->priority = priority;
		ret = 0;
	}

	return ret;
}

/*
 * Operation on a process or thread.
 */
int sys_prctl(int option, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5)
{
	UNUSED(arg2);
	UNUSED(arg3);
	UNUSED(arg4);
	UNUSED(arg5);

	switch (option) {
		case PR_GET_DUMPABLE:
			break;
		default:
			printf("Prctl: unknown option %d\n");
	}

	return 0;
}
