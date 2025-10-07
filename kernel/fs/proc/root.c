#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

/*
 * Root entries.
 */
struct proc_dir_entry proc_root = {
	PROC_ROOT_INO, 5, "/proc", S_IFDIR | S_IRUGO | S_IXUGO, 2, 0, 0, 0,
	&proc_root_iops, NULL, &proc_root, NULL, NULL
};
struct proc_dir_entry *proc_net;

/*
 * Init root entries.
 */
void proc_root_init()
{
	proc_misc_init();
	proc_net = proc_mkdir("net", 0);
}

/*
 * Root read dir.
 */
static int proc_root_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	size_t nr = filp->f_pos;
	int ret;

	/* read global entries */
	if (nr < FIRST_PROCESS_ENTRY) {
		ret = proc_readdir(filp, dirent, filldir);
		if (ret <= 0)
			return ret;
		filp->f_pos = nr = FIRST_PROCESS_ENTRY;
	}

	/* read pid entries */
	return proc_pid_readdir(filp, dirent, filldir);

}

/*
 * Root dir lookup.
 */
static struct dentry *proc_root_lookup(struct inode *dir, struct dentry *dentry)
{
	/* find matching entry */
	if (!proc_lookup(dir, dentry))
		return NULL;

	/* find matching <pid> entry */
	return proc_pid_lookup(dir, dentry);
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