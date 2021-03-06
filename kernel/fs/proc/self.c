#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>

/*
 * Follow self link.
 */
static int proc_self_follow_link(struct inode_t *dir, struct inode_t *inode, struct inode_t **res_inode)
{
	UNUSED(dir);

	/* get target inode */
	*res_inode = iget(inode->i_sb, PROC_BASE_INO + current_task->pid);

	/* release inode */
	iput(inode);

	if (!*res_inode)
		return -ENOENT;

	return 0;
}

/*
 * Read self link.
 */
static ssize_t proc_self_readlink(struct inode_t *inode, char *buf, size_t bufsize)
{
	char tmp[32];
	size_t len;

	/* release inode */
	iput(inode);

	/* set target link */
	len = sprintf(tmp, "%d", current_task->pid) + 1;
	if (bufsize < len)
		len = bufsize;

	/* copy target link */
	memcpy(buf, tmp, len);

	return len;
}

/*
 * Self inode operations.
 */
struct inode_operations_t proc_self_iops = {
	.readlink	= proc_self_readlink,
	.follow_link	= proc_self_follow_link,
};

