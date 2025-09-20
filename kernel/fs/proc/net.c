#include <fs/proc_fs.h>
#include <net/inet/net.h>
#include <fcntl.h>
#include <stdio.h>
#include <stderr.h>

#define NR_NET_DIRENTRY		(sizeof(net_dir) / sizeof(net_dir[0]))

/*
 * Net directory.
 */
static struct proc_dir_entry net_dir[] = {
	{ PROC_NET_INO,		1, 	"." },
	{ PROC_ROOT_INO,	2,	".." },
	{ PROC_NET_DEV_INO,	3,	"dev" },
};

/*
 * Read net dir.
 */
static int proc_net_getdents64(struct file *filp, void *dirp, size_t count)
{
	struct dirent64 *dirent = (struct dirent64 *) dirp;
	int ret, n;
	size_t i;

	/* read net dir entries */
	for (i = filp->f_pos, n = 0; i < NR_NET_DIRENTRY; i++, filp->f_pos++) {
		/* fill in directory entry */ 
		ret = filldir(dirent, net_dir[i].name, net_dir[i].name_len, net_dir[i].ino, count);
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
 * Lookup net dir.
 */
static struct dentry *proc_net_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	ino_t ino;
	size_t i;

	/* find matching entry */
	for (i = 0; i < NR_NET_DIRENTRY; i++) {
		if (proc_match(dentry->d_name.name, dentry->d_name.len, &net_dir[i])) {
			ino = net_dir[i].ino;
			goto found;
		}
	}

	return ERR_PTR(-ENOENT);
found:
	/* get inode */
	inode = iget(dir->i_sb, ino);
	if (!inode)
		return ERR_PTR(-EACCES);

	d_add(dentry, inode);
	return NULL;
}

/*
 * Net file operations.
 */
struct file_operations proc_net_fops = {
	.getdents64		= proc_net_getdents64,
};

/*
 * Net inode operations.
 */
struct inode_operations proc_net_iops = {
	.fops			= &proc_net_fops,
	.lookup			= proc_net_lookup,
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
