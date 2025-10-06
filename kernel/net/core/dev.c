#include <net/inet/net.h>
#include <fs/proc_fs.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>
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
 * Init network devices.
 */
int init_net_dev()
{
	struct proc_dir_entry *de;

	/* register net/dev procfs entry */
	de = create_proc_read_entry("dev", 0, proc_net, dev_read_proc);
	if (!de)
		return -EINVAL;

	return 0;
}