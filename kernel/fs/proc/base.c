#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <drivers/char/tty.h>
#include <string.h>
#include <stderr.h>
#include <fcntl.h>
#include <stdio.h>
#include <dev.h>

#define NR_BASE_DIRENTRY		(sizeof(base_dir) / sizeof(base_dir[0]))

/*
 * Base process directory.
 */
static struct proc_dir_entry base_dir[] = {
	{ PROC_PID_INO,		1,	"." },
	{ PROC_ROOT_INO,	2,	".." },
	{ PROC_PID_STAT_INO,	4,	"stat" },
	{ PROC_PID_STATUS_INO,	6,	"status" },
	{ PROC_PID_CMDLINE_INO,	7,	"cmdline" },
	{ PROC_PID_ENVIRON_INO,	7,	"environ" },
	{ PROC_PID_FD_INO,	2,	"fd" },
};

/*
 * Process states.
 */
static char proc_states[] = {
	'R',				/* running */
	'S',				/* sleeping */
	'T',				/* stopped */
	'Z',				/* zombie */
};

/*
 * Process states.
 */
static char *proc_states_desc[] = {
	"R (running)",			/* running */
	"S (sleeping)",			/* sleeping */
	"T (stopped)",			/* stopped */
	"Z' (zombie)"			/* zombie */
};

/*
 * Read process stat.
 */
static int proc_stat_read(struct task *task, char *page)
{
	struct list_head *pos;
	struct vm_area *vma;
	size_t vsize = 0;

	/* compute virtual memory */
	list_for_each(pos, &task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		vsize += vma->vm_end - vma->vm_start;
	}

	/* print informations in temporary buffer */
	return sprintf(page,	"%d (%s) "						/* pid, name */
				"%c %d "						/* state, ppid */
				"%d %d "						/* pgrp, session */
				"%d "							/* tty */
				"0 0 "							/* tpgid, flags */
				"0 0 0 0 "						/* minflt, cminflt, majflt, cmajflt */
				"%lld %lld %lld %lld "					/* utime, stime, cutime, cstime */
				"0 0 "							/* priority, nice */
				"0 0 "							/* num_threads, itrealvalue */
				"%lld "							/* starttime */
				"%d %d 0 \n",						/* vsize, rss, rsslim */
				task->pid, task->name,
				proc_states[task->state - 1], task->parent ? task->parent->pid : task->pid,
				task->pgrp, task->session,
				task->tty ? dev_t_to_nr(task->tty->device) : 0,
				task->utime, task->stime, task->cutime, task->cstime,
				task->start_time,
				vsize, task->mm->rss
			);
}

/*
 * Get task name.
 */
static char *task_name(struct task *task, char *buf)
{
	return buf + sprintf(buf, 	"Name:\t%s\n", task->name);
}

/*
 * Get task state.
 */
static char *task_state(struct task *task, char *buf)
{
	return buf + sprintf(buf, 	"State:\t%s\n"
					"Tgid:\t0\n"
					"Ngid:\t0\n"
					"Pid:\t%d\n"
					"PPid:\t%d\n"
					"TracerPid:\t0\n"
					"Uid:\t%d\t%d\t%d\t%d\n"
					"Gid:\t%d\t%d\t%d\t%d\n"
					"FDSize:\t0\n"
					"Groups:\t0\n",
					proc_states_desc[task->state - 1],
					task->pid,
					task->parent ? task->parent->pid : task->pid,
					task->uid, task->euid, task->suid, task->suid,
					task->gid, task->egid, task->sgid, task->sgid
				);
}

/*
 * Get task memory.
 */
static char *task_mem(struct task *task, char *buf)
{
	size_t len, vsize = 0, data = 0, stack = 0, exec = 0, lib = 0;
	struct list_head *pos;
	struct vm_area *vma;

	/* compute virtual memory */
	list_for_each(pos, &task->mm->vm_list) {
		vma = list_entry(pos, struct vm_area, list);
		len = (vma->vm_end - vma->vm_start) >> 10;

		/* update virtual size */
		vsize += len;

		/* update data/stack size */
		if (!vma->vm_inode) {
			if (vma->vm_flags & VM_GROWSDOWN)
				stack += len;
			else
				data += len;

			continue;
		}

		/* update exec/lib size */
		if (!(vma->vm_flags & VM_WRITE) && vma->vm_flags & VM_EXEC) {
			if (vma->vm_flags & VM_EXECUTABLE)
				exec += len;
			else
				lib += len;
		}
	}

	return buf + sprintf(buf, 	"VmSize:\t%d kB\n"
					"VmLck:\t0 kB\n"
					"VmRSS:\t%d kB\n"
					"VmData:\t%d kB\n"
					"VmStk:\t%d kB\n"
					"VmExe:\t%d kB\n"
					"VmLib:\t%d kB\n",
					vsize,
					task->mm->rss << (PAGE_SHIFT - 10),
					data,
					stack,
					exec,
					lib
				);
}

/*
 * Read process status.
 */
static int proc_status_read(struct task *task, char *page)
{
	char *ori = page;

	page = task_name(task, page);
	page = task_state(task, page);
	page = task_mem(task, page);

	return page - ori;
}

/*
 * Read process command line.
 */
static int proc_cmdline_read(struct task *task, char *page)
{
	char *p, *arg_str;
	uint32_t arg;

	/* switch to task's pgd */
	switch_pgd(task->mm->pgd);

	/* get arguments */
	for (arg = task->mm->arg_start, p = page; arg != task->mm->arg_end; arg += sizeof(char *)) {
		arg_str = *((char **) arg);

		/* copy argument */
		while (*arg_str && p - page < PAGE_SIZE)
			*p++ = *arg_str++;

		/* overflow */
		if (p - page >= PAGE_SIZE)
			break;

		/* end argument */
		*p++ = 0;
	}

	/* switch back to current's pgd */
	switch_pgd(current_task->mm->pgd);

	return p - page;
}

/*
 * Read process environ.
 */
static int proc_environ_read(struct task *task, char *page)
{
	char *p, *environ_str;
	uint32_t environ;

	/* switch to task's pgd */
	switch_pgd(task->mm->pgd);

	/* get environs */
	for (environ = task->mm->env_start, p = page; environ != task->mm->env_end; environ += sizeof(char *)) {
		environ_str = *((char **) environ);

		/* copy environ */
		while (*environ_str && p - page < PAGE_SIZE)
			*p++ = *environ_str++;

		/* overflow */
		if (p - page >= PAGE_SIZE)
			break;

		/* end environ */
		*p++ = 0;
	}

	/* switch back to current's pgd */
	switch_pgd(current_task->mm->pgd);

	return p - page;
}

/*
 * Read array.
 */
static int proc_base_read(struct file *filp, char *buf, int count)
{
	struct task *task;
	char *page;
	size_t len;
	ino_t ino;
	pid_t pid;

	/* get inode number */
	ino = filp->f_inode->i_ino & 0x0000FFFF;
	pid = filp->f_inode->i_ino >> 16;

	/* find task */
	task = find_task(pid);
	if (!task)
		return -EINVAL;

	/* get a free page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* get informations */
	switch (ino) {
		case PROC_PID_STAT_INO:
			len = proc_stat_read(task, page);
			break;
		case PROC_PID_STATUS_INO:
			len = proc_status_read(task, page);
			break;
		case PROC_PID_CMDLINE_INO:
			len = proc_cmdline_read(task, page);
			break;
		case PROC_PID_ENVIRON_INO:
			len = proc_environ_read(task, page);
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
 * Base file operations.
 */
struct file_operations proc_base_fops = {
	.read		= proc_base_read,
};

/*
 * Base inode operations.
 */
struct inode_operations proc_base_iops = {
	.fops		= &proc_base_fops,
};

/*
 * Get directory entries.
 */
static int proc_base_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct dirent64 *dirent;
	int n, ret;
	pid_t pid;
	size_t i;

	/* get pid */
	pid = filp->f_inode->i_ino >> 16;

	/* read root dir entries */
	for (i = filp->f_pos, n = 0, dirent = (struct dirent64 *) dirp; i < NR_BASE_DIRENTRY; i++, filp->f_pos++) {
		/* fill in directory entry */ 
		ret = filldir(dirent, base_dir[i].name, base_dir[i].name_len, (pid << 16) + base_dir[i].ino, count);
		if (ret)
			return n;

		/* go to next dir entry */
		count -= dirent->d_reclen;
		n += dirent->d_reclen;
		dirent = (struct dirent64 *) ((void *) dirent + dirent->d_reclen);
	}

	return n;
}

/*
 * Lookup for a file.
 */
static int proc_base_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode)
{
	struct proc_dir_entry *de;
	ino_t ino;
	size_t i;

	/* dir must be a directory */
	if (!dir)
		return -ENOENT;
	if (!S_ISDIR(dir->i_mode)) {
		iput(dir);
		return -ENOENT;
	}

	/* find matching entry */
	for (i = 0, de = NULL; i < NR_BASE_DIRENTRY; i++) {
		if (proc_match(name, name_len, &base_dir[i])) {
			de = &base_dir[i];
			break;
		}
	}

	/* no such entry */
	if (!de) {
		iput(dir);
		return -ENOENT;
	}

	/* create a fake inode */
	if (de->ino == 1)
		ino = 1;
	else
		ino = dir->i_ino - PROC_PID_INO + de->ino;

	/* get inode */
	*res_inode = iget(dir->i_sb, ino);
	if (!*res_inode) {
		iput(dir);
		return -EACCES;
	}

	iput(dir);
	return 0;
}

/*
 * Process file operations.
 */
struct file_operations proc_base_dir_fops = {
	.getdents64		= proc_base_getdents64,
};

/*
 * Process inode operations.
 */
struct inode_operations proc_base_dir_iops = {
	.fops			= &proc_base_dir_fops,
	.lookup			= proc_base_lookup,
};
