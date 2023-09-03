#include <drivers/char/zero.h>
#include <sys/syscall.h>
#include <fs/dev_fs.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>

/*
 * Open zero device.
 */
static int zero_open(struct file *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Close zero device.
 */
static int zero_close(struct file *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Read zero device.
 */
static int zero_read(struct file *filp, char *buf, int n)
{
	UNUSED(filp);
	memset(buf, 0, n);
	return n;
}

/*
 * Write to zero device.
 */
static int zero_write(struct file *filp, const char *buf, int n)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	return n;
}

/*
 * Zero device file operations.
 */
static struct file_operations zero_fops = {
	.open		= zero_open,
	.close		= zero_close,
	.read		= zero_read,
	.write		= zero_write,
};

/*
 * Zero device inode operations.
 */
struct inode_operations zero_iops = {
	.fops		= &zero_fops,
};

/*
 * Init zero device.
 */
int init_zero()
{
	if (!devfs_register(NULL, "zero", S_IFCHR | 0666, DEV_ZERO))
		return -ENOSPC;

	return 0;
}
