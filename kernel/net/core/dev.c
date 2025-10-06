#include <net/inet/net.h>
#include <fs/proc_fs.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

/*
 * Read net dev.
 */
static int dev_read_proc(char *page, char **start, off_t off, size_t count, int *eof)
{
	size_t len;
	int i;

	/* print header */
	len = sprintf(page, "Inter-|   Receive                            "
		"                    |  Transmit\n"
		" face |bytes    packets errs drop fifo frame "
		"compressed multicast|bytes    packets errs "
		"drop fifo colls carrier compressed\n");

	/* print interfaces */
	for (i = 0; i < nr_net_devices; i++)
		len += sprintf(page + len, "%s: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n", net_devices[i].name);

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
 * Net dev procfs entry.
 */
static struct proc_dir_entry proc_net_dev = {
	PROC_NET_DEV_INO, 3, "dev", S_IFREG | S_IRUGO, 1, 0, 0, 0,
	&proc_file_iops, dev_read_proc, NULL, NULL, NULL
};

/*
 * Init network devices.
 */
int init_net_dev()
{
	/* register net/dev procfs entry */
	return proc_net_register(&proc_net_dev);
}