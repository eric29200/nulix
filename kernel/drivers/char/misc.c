#include <drivers/char/misc.h>
#include <stderr.h>
#include <dev.h>

/* misc devices */
static LIST_HEAD(misc_devices);

/*
 * Register a misc device.
 */
int misc_register(struct misc_device *misc)
{
	struct list_head *pos;
	struct misc_device *m;

	/* check if device is already present */
	list_for_each(pos, &misc_devices) {
		m = list_entry(pos, struct misc_device, list);
		if (m->minor == misc->minor)
			return -EBUSY;
	}

	/* add misc device */
	list_add_tail(&misc->list, &misc_devices);

	return 0;
}

/*
 * Deregister a misc device.
 */
int misc_deregister(struct misc_device *misc)
{
	list_del(&misc->list);
	return 0;
}

/*
 * Open a misc device.
 */
static int misc_open(struct inode *inode, struct file *filp)
{
	struct misc_device *misc;
	struct list_head *pos;
	int minor;

	/* check inode */
	if (!inode)
		return -EINVAL;

	/* find misc device */
	minor = minor(inode->i_rdev);
	list_for_each(pos, &misc_devices) {
		misc = list_entry(pos, struct misc_device, list);
		if (misc->minor == minor && misc->fops)
			goto found;
	}

	return -ENODEV;
found:
	filp->f_op = misc->fops;
	if (filp->f_op->open)
		return filp->f_op->open(inode, filp);
	return 0;
}

/*
 * Misc device file operations.
 */
static struct file_operations misc_fops = {
	.open		= misc_open,
};

/*
 * Init misc devices.
 */
int init_misc_devices()
{
	return register_chrdev(DEV_MISC_MAJOR, "misc", &misc_fops);
}