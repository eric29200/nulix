#include <fs/proc_fs.h>
#include <net/inet/net.h>
#include <fcntl.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Net file operations.
 */
struct file_operations proc_net_fops = {
	.readdir		= proc_readdir,
};

/*
 * Net inode operations.
 */
struct inode_operations proc_net_iops = {
	.fops			= &proc_net_fops,
	.lookup			= proc_lookup,
};

/*
 * Read net dev.
 */
static int proc_net_dev_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	char *page;
	size_t len;
	int i;

	/* unused filp */
	UNUSED(filp);

	/* get a page */
	page = get_free_page();
	if (!page)
		return -ENOMEM;

	/* print header */
	len = sprintf(page, "Inter-|   Receive                            "
		"                    |  Transmit\n"
		" face |bytes    packets errs drop fifo frame "
		"compressed multicast|bytes    packets errs "
		"drop fifo colls carrier compressed\n");

	/* print interfaces */
	for (i = 0; i < nr_net_devices; i++)
		len += sprintf(page + len, "%s: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n", net_devices[i].name);

	/* file position after end */
	if (*ppos >= len) {
		count = 0;
		goto out;
	}

	/* update count */
	if (*ppos + count > len)
		count = len - *ppos;

	/* copy content to user buffer and update file position */
	memcpy(buf, page + *ppos, count);
	*ppos += count;
out:
	free_page(page);
	return count;
}

/*
 * Net dev file operations.
 */
struct file_operations proc_net_dev_fops = {
	.read		= proc_net_dev_read,
};

/*
 * Net dev inode operations.
 */
struct inode_operations proc_net_dev_iops = {
	.fops		= &proc_net_dev_fops,
};
