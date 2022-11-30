#include <fs/fs.h>
#include <drivers/ata.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>

/*
 * Get block device driver.
 */
struct inode_operations_t *block_get_driver(struct inode_t *inode)
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
int generic_block_read(struct file_t *filp, char *buf, int count)
{
	size_t blocksize, pos, nb_chars, left;
	struct buffer_head_t *bh;
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
		nb_chars = blocksize - pos <= left ? blocksize - pos : left;

		/* copy into buffer */
		memcpy(buf, bh->b_data + pos, nb_chars);

		/* release block */
		brelse(bh);

		/* update sizes */
		filp->f_pos += nb_chars;
		buf += nb_chars;
		left -= nb_chars;
	}

	return count - left;
}