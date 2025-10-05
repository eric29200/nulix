#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>

/*
 * Follow self link.
 */
static struct dentry *proc_self_follow_link(struct dentry *dentry, struct dentry *base)
{
	char tmp[30];

	/* unused dentry */
	UNUSED(dentry);

	sprintf(tmp, "%d", current_task->pid);
	return lookup_dentry(AT_FDCWD, base, tmp, 1);
}

/*
 * Read self link.
 */
static ssize_t proc_self_readlink(struct inode *inode, char *buf, size_t bufsize)
{
	char tmp[32];
	size_t len;

	/* unused inode */
	UNUSED(inode);

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
struct inode_operations proc_self_iops = {
	.readlink	= proc_self_readlink,
	.follow_link	= proc_self_follow_link,
};

