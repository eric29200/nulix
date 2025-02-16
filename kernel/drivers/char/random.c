#include <drivers/char/random.h>
#include <sys/syscall.h>
#include <fs/dev_fs.h>
#include <stderr.h>
#include <fcntl.h>
#include <math.h>
#include <dev.h>

/*
 * Process unaligned data.
 */
static void random_unaligned(uint8_t *buf, size_t n)
{
    uint32_t r;
    size_t i;

    r = rand();
    for (i = 0; i < n; i++) {
        *buf++ = (uint8_t) r;
        r >>= 8;
    }
}

/*
 * Open random device.
 */
static int random_open(struct file *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Close random device.
 */
static int random_close(struct file *filp)
{
	UNUSED(filp);
	return 0;
}

/*
 * Read random device.
 */
static int random_read(struct file *filp, char *buf, int n)
{
	uint32_t *buf32;
	size_t iter, i;

	/* unused filp */
	UNUSED(filp);

	/* eventually process front unaligned data */
	if (((iter = ((uint32_t) buf & 0x03))) != 0)
		random_unaligned((uint8_t *) buf, sizeof(uint32_t) - iter);

	/* at this point, buf is be aligned to a uint32_t boundary (safe cast) */
	iter = (n >> 2);
	buf32 = (uint32_t *) ALIGN_UP((uint32_t) buf, sizeof(uint32_t));
	for (i = 0; i < iter; i++)
		buf32[i] = rand();

	/* eventually process back unaligned data */
	if ((iter = (n & 0x03)) != 0)
		random_unaligned((unsigned char *)&buf32[i], iter);

    	return n;
}

/*
 * Write to random device.
 */
static int random_write(struct file *filp, const char *buf, int n)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	return n;
}

/*
 * Random device file operations.
 */
static struct file_operations random_fops = {
	.open		= random_open,
	.close		= random_close,
	.read		= random_read,
	.write		= random_write,
};

/*
 * Random device inode operations.
 */
struct inode_operations random_iops = {
	.fops		= &random_fops,
};

/*
 * Init random device.
 */
int init_random()
{
	if (!devfs_register(NULL, "random", S_IFCHR | 0666, DEV_RANDOM))
		return -ENOSPC;

	if (!devfs_register(NULL, "urandom", S_IFCHR | 0666, DEV_URANDOM))
		return -ENOSPC;

	return 0;
}
