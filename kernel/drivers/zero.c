#include <drivers/zero.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <dev.h>

/*
 * Open zero device.
 */
static int zero_open(struct file_t *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Close zero device.
 */
static int zero_close(struct file_t *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Read zero device.
 */
static int zero_read(struct file_t *filp, char *buf, int n)
{
	UNUSED(filp);
	memset(buf, 0, n);
	return n;
}

/*
 * Write to zero device.
 */
static int zero_write(struct file_t *filp, const char *buf, int n)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	return n;
}

/*
 * Zero device file operations.
 */
static struct file_operations_t zero_fops = {
	.open		= zero_open,
	.close		= zero_close,
	.read		= zero_read,
	.write		= zero_write,
};

/*
 * Zero device inode operations.
 */
struct inode_operations_t zero_iops = {
	.fops		= &zero_fops,
};

/*
 * Init zero device.
 */
int init_zero_dev()
{
	return sys_mknod("/dev/zero", S_IFCHR | 0666, DEV_ZERO);
}