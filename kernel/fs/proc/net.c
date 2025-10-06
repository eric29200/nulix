#include <fs/proc_fs.h>
#include <net/inet/net.h>
#include <fcntl.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Net file operations.
 */
static struct file_operations proc_net_fops = {
	.readdir		= proc_readdir,
};

/*
 * Net inode operations.
 */
struct inode_operations proc_net_iops = {
	.fops			= &proc_net_fops,
	.lookup			= proc_lookup,
};