#include <fs/fs.h>
#include <stdio.h>
#include <stderr.h>
#include <dev.h>
/*
 * Device structure.
 */
struct device {
	const char *			name;
	struct file_operations *	fops;
};

/*
 * Character devices.
 */
static struct device chrdevs[MAX_CHRDEV] = {
	{ NULL, NULL },
};

/*
 * Block devices.
 */
static struct device blkdevs[MAX_BLKDEV] = {
	{ NULL, NULL },
};

/*
 * Register a character device.
 */
int register_chrdev(int major, const char *name, struct file_operations *fops)
{
	/* check major number */
	if (major >= MAX_CHRDEV)
		return -EINVAL;

	/* device already registered */
	if (chrdevs[major].fops && chrdevs[major].fops != fops)
		return -EBUSY;

	/* register device */
	chrdevs[major].name = name;
	chrdevs[major].fops = fops;
	return 0;
}

/*
 * Register a block device.
 */
int register_blkdev(int major, const char *name, struct file_operations *fops)
{
	/* check major number */
	if (major >= MAX_BLKDEV)
		return -EINVAL;

	/* device already registered */
	if (blkdevs[major].fops && blkdevs[major].fops != fops)
		return -EBUSY;

	/* register device */
	blkdevs[major].name = name;
	blkdevs[major].fops = fops;
	return 0;
}

/*
 * Unregister a character device.
 */
int unregister_chrdev(int major, const char *name)
{
	/* check major number */
	if (major >= MAX_CHRDEV)
		return -EINVAL;

	/* device not regeistered */
	if (!chrdevs[major].fops)
		return -EINVAL;

	/* check name */
	if (strcmp(chrdevs[major].name, name))
		return -EINVAL;

	/* unregister device */
	chrdevs[major].name = NULL;
	chrdevs[major].fops = NULL;
	return 0;
}

/*
 * Unregister a block device.
 */
int unregister_blkdev(int major, const char *name)
{
	/* check major number */
	if (major >= MAX_BLKDEV)
		return -EINVAL;

	/* device not regeistered */
	if (!blkdevs[major].fops)
		return -EINVAL;

	/* check name */
	if (strcmp(blkdevs[major].name, name))
		return -EINVAL;

	/* unregister device */
	blkdevs[major].name = NULL;
	blkdevs[major].fops = NULL;
	return 0;
}

/*
 * Open a character device.
 */
static int chrdev_open(struct file *filp)
{
	struct dentry *dentry;
	struct inode *inode;
	int major;

	/* get dentry */
	dentry = filp->f_dentry;
	if (!dentry)
		return -EINVAL;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		return -EINVAL;

	/* check block device */
	major = major(inode->i_rdev);
	if (major >= MAX_CHRDEV || !chrdevs[major].fops)
		return -ENODEV;

	/* set file operations */
	filp->f_op = chrdevs[major].fops;

	/* specific open */
	if (filp->f_op->open)
		return filp->f_op->open(filp);

	return 0;
}

/*
 * Open a block device.
 */
static int blkdev_open(struct file *filp)
{
	struct dentry *dentry;
	struct inode *inode;
	int major;

	/* get dentry */
	dentry = filp->f_dentry;
	if (!dentry)
		return -EINVAL;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		return -EINVAL;

	/* check block device */
	major = major(inode->i_rdev);
	if (major >= MAX_BLKDEV || !blkdevs[major].fops)
		return -ENODEV;

	/* set file operations */
	filp->f_op = blkdevs[major].fops;

	/* specific open */
	if (filp->f_op->open)
		return filp->f_op->open(filp);

	return 0;
}

/*
 * Character device file operations.
 */
static struct file_operations chrdev_fops = {
	.open			= chrdev_open,
};

/*
 * Character device inode operations.
 */
struct inode_operations chrdev_iops = {
	.fops			= &chrdev_fops,
};

/*
 * Block device file operations.
 */
static struct file_operations blkdev_fops = {
	.open			= blkdev_open,
};

/*
 * Block device inode operations.
 */
struct inode_operations blkdev_iops = {
	.fops			= &blkdev_fops,
};
