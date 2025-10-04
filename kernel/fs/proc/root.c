#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#define FIRST_PROCESS_ENTRY		256
#define PROC_MAXPIDS			20
#define PROC_NUMBUF			10

/*
 * Root entries.
 */
struct proc_dir_entry proc_root = {
	PROC_ROOT_INO, 5, "/proc", NULL, &proc_root, NULL
};
static struct proc_dir_entry proc_root_uptime = {
	PROC_UPTIME_INO, 6, "uptime", NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_filesystems = {
	PROC_FILESYSTEMS_INO, 11, "filesystems", NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_mounts = {
	PROC_MOUNTS_INO, 6, "mounts", NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_stat = {
	PROC_KSTAT_INO, 4, "stat", NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_self = {
	PROC_SELF_INO, 4, "self", NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_meminfo = {
	PROC_MEMINFO_INO, 7, "meminfo", NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_loadavg = {
	PROC_LOADAVG_INO, 7, "loadavg", NULL, NULL, NULL
};
struct proc_dir_entry proc_root_net = {
	PROC_NET_INO, 3, "net", NULL, NULL, NULL
};

/*
 * Net entries.
 */
static struct proc_dir_entry proc_net_dev = {
	PROC_NET_DEV_INO, 3, "dev", NULL, NULL, NULL
};

/*
 * Register a proc filesystem entry.
 */
int proc_register(struct proc_dir_entry *dir, struct proc_dir_entry *de)
{
	/* set parent */
	de->parent = dir;

	/* add entry to parent */
	de->next = dir->subdir;
	dir->subdir = de;

	return 0;
}

/*
 * Init root entries.
 */
void proc_root_init()
{
	proc_base_init();
	proc_register(&proc_root, &proc_root_uptime);
	proc_register(&proc_root, &proc_root_filesystems);
	proc_register(&proc_root, &proc_root_mounts);
	proc_register(&proc_root, &proc_root_stat);
	proc_register(&proc_root, &proc_root_self);
	proc_register(&proc_root, &proc_root_meminfo);
	proc_register(&proc_root, &proc_root_loadavg);
	proc_register(&proc_root, &proc_root_net);
	proc_register(&proc_root_net, &proc_net_dev);
}

/*
 * Generic procfs read directory.
 */
int proc_readdir(struct file *filp, void *dirent, filldir_t filldir, struct proc_dir_entry *de)
{
	struct inode *inode = filp->f_dentry->d_inode;
	ino_t ino = inode->i_ino;
	size_t i = filp->f_pos;

	/* "." entry */
	if (i == 0) {
		if (filldir(dirent, ".", 1, i, inode->i_ino))
			return 0;
		i++;
		filp->f_pos++;
	}

	/* ".." entry */
	if (i == 1) {
		if (filldir(dirent, "..", 2, i, de->parent->ino))
			return 0;
		i++;
		filp->f_pos++;
	}

	/* skip first entries */
	ino &= ~0xFFFF;
	de = de->subdir;
	i -= 2;
	for (;;) {
		if (!de)
			return 1;
		if (!i)
			break;
		de = de->next;
		i--;
	}

	/* read entries */
	do {
		if (filldir(dirent, de->name, de->name_len, filp->f_pos, ino | de->ino))
			return 0;
		filp->f_pos++;
		de = de->next;
	} while (de);

	return 0;
}

/*
 * Generic procfs read directory.
 */
struct dentry *proc_lookup(struct inode *dir, struct dentry *dentry, struct proc_dir_entry *de)
{
	struct inode *inode = NULL;
	int ret = -ENOENT;
	ino_t ino;

	/* find matching entry */
	if (de) {
		for (de = de->subdir; de != NULL; de = de->next) {
			if (proc_match(dentry->d_name.name, dentry->d_name.len, de)) {
				ino = de->ino | (dir->i_ino & ~(0xFFFF));
				ret = -EINVAL;
				inode = iget(dir->i_sb, ino);
				break;
			}
		}
	}

	/* found inode */
	if (inode) {
		d_add(dentry, inode);
		return NULL;
	}

	return ERR_PTR(ret);
}

/*
 * Get pids list.
 */
static int get_pid_list(int index, ino_t *pids)
{
	struct list_head *pos;
	struct task *task;
	int nr_pids = 0;

	index -= FIRST_PROCESS_ENTRY;

	list_for_each(pos, &tasks_list) {
		/* skip init task */
		task = list_entry(pos, struct task, list);
		if (!task || !task->pid)
			continue;

		/* skip first tasks */
		if (--index >= 0)
			continue;

		/* add task */
		pids[nr_pids++] = task->pid;
		if (nr_pids >= PROC_MAXPIDS)
			break;
	}

	return nr_pids;
}

/*
 * Root read dir.
 */
static int proc_root_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	size_t nr = filp->f_pos, nr_pids, i, j;
	ino_t pid_array[PROC_MAXPIDS];
	char buf[PROC_NUMBUF];
	pid_t pid;
	ino_t ino;
	int ret;

	/* read global entries */
	if (nr < FIRST_PROCESS_ENTRY) {
		ret = proc_readdir(filp, dirent, filldir, &proc_root);
		if (ret <= 0)
			return ret;
		filp->f_pos = nr = FIRST_PROCESS_ENTRY;
	}

	/* get pids list */
	nr_pids = get_pid_list(nr, pid_array);

	/* add pids entries */
	for (i = 0; i < nr_pids; i++) {
		pid = pid_array[i];
		ino = (pid << 16) + PROC_PID_INO;
		j = PROC_NUMBUF;

		do {
			j--;
			buf[j] = '0' + (pid % 10);
			pid /= 10;
		} while (pid);

		if (filldir(dirent, buf+j, PROC_NUMBUF-j, filp->f_pos, ino) < 0)
			break;

		filp->f_pos++;
	}

	return 0;
}

/*
 * Root dir lookup.
 */
static struct dentry *proc_root_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct task *task;
	pid_t pid;
	ino_t ino;

	/* find matching entry */
	if (!proc_lookup(dir, dentry, &proc_root))
		return NULL;

	/* else try to find matching process */
	pid = atoi(dentry->d_name.name);
	task = find_task(pid);
	if (!pid || !task)
		return ERR_PTR(-ENOENT);

	/* get inode */
	ino = (task->pid << 16) + PROC_PID_INO;
	inode = iget(dir->i_sb, ino);
	if (!inode)
		return ERR_PTR(-EACCES);

	d_add(dentry, inode);
	return NULL;
}

/*
 * Root file operations.
 */
struct file_operations proc_root_fops = {
	.readdir		= proc_root_readdir,
};

/*
 * Root inode operations.
 */
struct inode_operations proc_root_iops = {
	.fops			= &proc_root_fops,
	.lookup			= proc_root_lookup,
};

