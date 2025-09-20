#include <drivers/char/mem.h>
#include <stdio.h>
#include <stderr.h>
#include <math.h>
#include <dev.h>

/*
 * Read null device.
 */
static int null_read(struct file *filp, char *buf, size_t n, off_t *ppos)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	UNUSED(ppos);
	return 0;
}

/*
 * Write to null device.
 */
static int null_write(struct file *filp, const char *buf, size_t n, off_t *ppos)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	UNUSED(ppos);
	return n;
}

/*
 * Read zero device.
 */
static int zero_read(struct file *filp, char *buf, size_t n, off_t *ppos)
{
	UNUSED(filp);
	UNUSED(ppos);
	memset(buf, 0, n);
	return n;
}

/*
 * Write to zero device.
 */
static int zero_write(struct file *filp, const char *buf, size_t n, off_t *ppos)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	UNUSED(ppos);
	return n;
}

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
 * Read random device.
 */
int random_read(struct file *filp, char *buf, size_t n, off_t *ppos)
{
	uint32_t *buf32;
	size_t iter, i;

	/* unused filp */
	UNUSED(filp);
	UNUSED(ppos);

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
static int random_write(struct file *filp, const char *buf, size_t n, off_t *ppos)
{
	UNUSED(filp);
	UNUSED(buf);
	UNUSED(n);
	UNUSED(ppos);
	return n;
}

/*
 * Null device file operations.
 */
static struct file_operations null_fops = {
	.read		= null_read,
	.write		= null_write,
};

/*
 * Zero device file operations.
 */
static struct file_operations zero_fops = {
	.read		= zero_read,
	.write		= zero_write,
};

/*
 * Random device file operations.
 */
static struct file_operations random_fops = {
	.read		= random_read,
	.write		= random_write,
};

/*
 * Open a memory device.
 */
static int memory_open(struct inode *inode, struct file *filp)
{
	/* check inode */
	if (!inode)
		return -EINVAL;

	/* get file operations */
	switch (minor(inode->i_rdev)) {
		case 3:
			filp->f_op = &null_fops;
			break;
		case 5:
			filp->f_op = &zero_fops;
			break;
		case 8:
		case 9:
			filp->f_op = &random_fops;
			break;
		default:
			return -ENXIO;
	}

	/* open specific device */
	if (filp->f_op && filp->f_op->open)
		return filp->f_op->open(inode, filp);

	return 0;
}

/*
 * Memory device file operations.
 */
static struct file_operations memory_fops = {
	.open		= memory_open,
};

/*
 * Init memory devices.
 */
int init_mem_devices()
{
	return register_chrdev(DEV_MEMORY_MAJOR, "mem", &memory_fops);
}