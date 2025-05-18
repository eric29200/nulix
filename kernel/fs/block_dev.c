#include <fs/fs.h>
#include <drivers/block/ata.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>
#include <stdio.h>
#include <string.h>

/*
 * Get block device driver.
 */
struct inode_operations *block_get_driver(struct inode *inode)
{
	/* not a block device */
	if (!inode || !S_ISBLK(inode->i_mode))
		return NULL;

	/* ata driver */
	if (major(inode->i_rdev) == DEV_ATA_MAJOR)
		return &ata_iops;

	return NULL;
}

/*
 * Generic block read.
 */
int generic_block_read(struct file *filp, char *buf, int count)
{
	size_t blocksize, pos, nr_chars, left;
	struct buffer_head *bh;
	dev_t dev;

	/* check size */
	if (count <= 0)
		return 0;

	/* get device and blocksize */
	dev = filp->f_inode->i_rdev;
	blocksize = blocksize_size[major(dev)][minor(dev)];
	if (!blocksize)
		return -EINVAL;

	left = count;
	while (left > 0) {
		/* read block */
		bh = bread(dev, filp->f_pos / blocksize, blocksize);
		if (!bh)
			break;

		/* find position and number of chars to read */
		pos = filp->f_pos % blocksize;
		nr_chars = blocksize - pos <= left ? blocksize - pos : left;

		/* copy into buffer */
		memcpy(buf, bh->b_data + pos, nr_chars);

		/* release block */
		brelse(bh);

		/* update sizes */
		filp->f_pos += nr_chars;
		buf += nr_chars;
		left -= nr_chars;
	}

	return count - left;
}

/*
 * Generic block write.
 */
int generic_block_write(struct file *filp, const char *buf, int count)
{
	size_t blocksize, pos, nr_chars, left;
	struct buffer_head *bh;
	dev_t dev;

	/* check size */
	if (count <= 0)
		return 0;

	/* get device and blocksize */
	dev = filp->f_inode->i_rdev;
	blocksize = blocksize_size[major(dev)][minor(dev)];
	if (!blocksize)
		return -EINVAL;

	left = count;
	while (left > 0) {
		/* read block */
		bh = bread(dev, filp->f_pos / blocksize, blocksize);
		if (!bh)
			break;

		/* find position and number of chars to write */
		pos = filp->f_pos % blocksize;
		nr_chars = blocksize - pos <= left ? blocksize - pos : left;

		/* copy into block buffer */
		memcpy(bh->b_data + pos, buf, nr_chars);

		/* release block */
		bh->b_dirt = 1;
		brelse(bh);

		/* update sizes */
		filp->f_pos += nr_chars;
		buf += nr_chars;
		left -= nr_chars;
	}

	return count - left;
}

/*
 * Generic block write.
 */
int generic_block_ioctl(struct file *filp, int request, unsigned long arg)
{
	UNUSED(arg);

	printf("Unknown ioctl request (0x%x) on device 0x%x\n", request, (int) filp->f_inode->i_rdev);

	return 0;
}

/*
 * Read a block.
 */
int block_read(struct buffer_head *bh)
{
	switch (major(bh->b_dev)) {
		case DEV_ATA_MAJOR:
			return ata_read(bh);
		default:
			return -EINVAL;
	}
}

/*
 * Write a block.
 */
int block_write(struct buffer_head *bh)
{
	switch (major(bh->b_dev)) {
		case DEV_ATA_MAJOR:
			return ata_write(bh);
		default:
			return -EINVAL;
	}
}
