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
	PROC_ROOT_INO, 5, "/proc", S_IFDIR | S_IRUGO | S_IXUGO, 2, 0, 0, 0,
	&proc_root_iops, NULL, &proc_root, NULL, NULL
};
static struct proc_dir_entry proc_root_uptime = {
	PROC_UPTIME_INO, 6, "uptime", S_IFREG | S_IRUSR, 1, 0, 0, 0,
	&proc_array_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_filesystems = {
	PROC_FILESYSTEMS_INO, 11, "filesystems", S_IFREG | S_IRUSR, 1, 0, 0, 0,
	&proc_array_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_mounts = {
	PROC_MOUNTS_INO, 6, "mounts", S_IFREG | S_IRUSR, 1, 0, 0, 0,
	&proc_array_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_stat = {
	PROC_KSTAT_INO, 4, "stat", S_IFREG | S_IRUSR, 1, 0, 0, 0,
	&proc_array_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_self = {
	PROC_SELF_INO, 4, "self", S_IFLNK | S_IRUGO | S_IWUGO | S_IXUGO, 1, 0, 0, 64,
	&proc_self_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_meminfo = {
	PROC_MEMINFO_INO, 7, "meminfo", S_IFREG | S_IRUSR, 1, 0, 0, 0,
	&proc_array_iops, NULL, NULL, NULL, NULL
};
static struct proc_dir_entry proc_root_loadavg = {
	PROC_LOADAVG_INO, 7, "loadavg", S_IFREG | S_IRUSR, 1, 0, 0, 0,
	&proc_array_iops, NULL, NULL, NULL, NULL
};
struct proc_dir_entry *proc_net;

/* dynamic inodes bitmap */
static uint8_t proc_alloc_map[PROC_NDYNAMIC / 8] = { 0 };

/*
 * Create a dynamic inode number.
 */
static int make_inode_number()
{
	int i;

	/* find first free inode */
	i = find_first_zero_bit((uint32_t *) proc_alloc_map, PROC_NDYNAMIC);
	if (i < 0 || i >= PROC_NDYNAMIC) 
		return -1;

	/* set inode */
	set_bit((uint32_t *) proc_alloc_map, i);
	return PROC_DYNAMIC_FIRST + i;
}

/*
 * Register a proc filesystem entry.
 */
int proc_register(struct proc_dir_entry *dir, struct proc_dir_entry *de)
{
	int i;

	/* create an inode number if needed */
	if (de->low_ino == 0) {
		i = make_inode_number();
		if (i < 0)
			return -EAGAIN;
		de->low_ino = i;
	}

	/* set parent */
	de->parent = dir;

	/* add entry to parent */
	de->next = dir->subdir;
	dir->subdir = de;

	/* set default operations */
	if (S_ISDIR(de->mode)) {
		if (!de->ops)
			de->ops = &proc_dir_iops;
		dir->nlink++;
	} else if (S_ISLNK(de->mode) && !de->ops) {
		de->ops = &proc_link_iops;
	} else if (!de->ops) {
			de->ops = &proc_file_iops;
	}

	return 0;
}

/*
 * Init root entries.
 */
void proc_root_init()
{
	/* init base */
	proc_base_init();

	/* register root entries */
	proc_register(&proc_root, &proc_root_uptime);
	proc_register(&proc_root, &proc_root_filesystems);
	proc_register(&proc_root, &proc_root_mounts);
	proc_register(&proc_root, &proc_root_stat);
	proc_register(&proc_root, &proc_root_self);
	proc_register(&proc_root, &proc_root_meminfo);
	proc_register(&proc_root, &proc_root_loadavg);
	proc_net = create_proc_entry("net", S_IFDIR, NULL);
}

/*
 * Generic procfs read directory.
 */
int proc_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct proc_dir_entry *de;
	ino_t ino = inode->i_ino;
	size_t i = filp->f_pos;

	/* get proc entry */
	de = (struct proc_dir_entry *) inode->u.generic_i;

	/* "." entry */
	if (i == 0) {
		if (filldir(dirent, ".", 1, i, inode->i_ino))
			return 0;
		i++;
		filp->f_pos++;
	}

	/* ".." entry */
	if (i == 1) {
		if (filldir(dirent, "..", 2, i, de->parent->low_ino))
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
		if (filldir(dirent, de->name, de->name_len, filp->f_pos, ino | de->low_ino))
			return 0;
		filp->f_pos++;
		de = de->next;
	} while (de);

	return 0;
}

/*
 * Generic procfs read directory.
 */
struct dentry *proc_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct proc_dir_entry *de;
	int ret = -ENOENT;
	ino_t ino;

	/* get proc entry */
	de = (struct proc_dir_entry *) dir->u.generic_i;

	/* find matching entry */
	if (de) {
		for (de = de->subdir; de != NULL; de = de->next) {
			if (proc_match(dentry->d_name.name, dentry->d_name.len, de)) {
				ino = de->low_ino | (dir->i_ino & ~(0xFFFF));
				ret = -EINVAL;
				inode = proc_get_inode(dir->i_sb, ino, de);
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
		ret = proc_readdir(filp, dirent, filldir);
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
	if (!proc_lookup(dir, dentry))
		return NULL;

	/* else try to find matching process */
	pid = atoi(dentry->d_name.name);
	task = find_task(pid);
	if (!pid || !task)
		return ERR_PTR(-ENOENT);

	/* get inode */
	ino = (task->pid << 16) + PROC_PID_INO;
	inode = proc_get_inode(dir->i_sb, ino, &proc_pid);
	if (!inode)
		return ERR_PTR(-EACCES);

	d_add(dentry, inode);
	return NULL;
}

/*
 * Root file operations.
 */
static struct file_operations proc_root_fops = {
	.readdir		= proc_root_readdir,
};

/*
 * Root inode operations.
 */
struct inode_operations proc_root_iops = {
	.fops			= &proc_root_fops,
	.lookup			= proc_root_lookup,
};

/*
 * Directory file operations.
 */
static struct file_operations proc_dir_fops = {
	.readdir		= proc_readdir,
};

/*
 * Directory inode operations.
 */
struct inode_operations proc_dir_iops = {
	.fops			= &proc_dir_fops,
	.lookup			= proc_lookup,
};

/*
 * Dynamic directory inode operations.
 */
struct inode_operations proc_dyna_dir_iops = {
	.fops			= &proc_dir_fops,
	.lookup			= proc_lookup,
};