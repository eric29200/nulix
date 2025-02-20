#include <drivers/char/null.h>
#include <sys/syscall.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>

/*
 * Open null device.
 */
static int null_open(struct file *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Close null device.
 */
static int null_close(struct file *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Read null device.
 */
static int null_read(struct file *filp, char *buf, int n)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	return 0;
}

/*
 * Write to null device.
 */
static int null_write(struct file *filp, const char *buf, int n)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	return n;
}

/*
 * Null device file operations.
 */
static struct file_operations null_fops = {
	.open		= null_open,
	.close		= null_close,
	.read		= null_read,
	.write		= null_write,
};

/*
 * Null device inode operations.
 */
struct inode_operations null_iops = {
	.fops		= &null_fops,
};
