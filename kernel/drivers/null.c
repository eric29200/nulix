#include <drivers/null.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <dev.h>

/*
 * Open null device.
 */
static int null_open(struct file_t *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Close null device.
 */
static int null_close(struct file_t *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Read null device.
 */
static int null_read(struct file_t *filp, char *buf, int n)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	return 0;
}

/*
 * Write to null device.
 */
static int null_write(struct file_t *filp, const char *buf, int n)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	return n;
}

/*
 * Null device file operations.
 */
static struct file_operations_t null_fops = {
	.open		= null_open,
	.close		= null_close,
	.read		= null_read,
	.write		= null_write,
};

/*
 * Null device inode operations.
 */
struct inode_operations_t null_iops = {
	.fops		= &null_fops,
};

/*
 * Init null device.
 */
int init_null_dev()
{
	return sys_mknod("/dev/null", S_IFCHR | 0666, DEV_NULL);
}