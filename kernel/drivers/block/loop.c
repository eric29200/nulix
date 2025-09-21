#include <drivers/block/loop.h>
#include <drivers/block/blk_dev.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>

#define MAX_LOOP	4
#define MAX_DISK_SIZE 	1024*1024*1024

/* global variables */
static struct loop_device loop_dev[MAX_LOOP];
static size_t loop_sizes[MAX_LOOP];
static size_t loop_blksizes[MAX_LOOP];

/*
 * Compute loop device size.
 */
static void figure_loop_size(struct loop_device *lo)
{
	size_t size;

	if (S_ISREG(lo->lo_dentry->d_inode->i_mode))
		size = (lo->lo_dentry->d_inode->i_size - lo->lo_offset) / BLOCK_SIZE;
	else if (blk_size[major(lo->lo_device)])
		size = blk_size[major(lo->lo_device)][minor(lo->lo_device)] - lo->lo_offset / BLOCK_SIZE;
	else
		size = MAX_DISK_SIZE;

	loop_sizes[lo->lo_number] = size;
}

/*
 * Create a missing block.
 */
static int create_missing_block(struct loop_device *lo, uint32_t block, size_t blocksize)
{
	char zero_buf[1] = { 0 };
	struct file *filp;
	off_t new_offset;
	int ret;

	/* get backing file */
	filp = lo->lo_backing_file;
	if (!filp || !filp->f_op)
		goto err;

	/* seek to block */
	new_offset = block * blocksize;
	if (filp->f_op->llseek)
		filp->f_op->llseek(filp, new_offset, 0);
	else
		filp->f_pos = new_offset;

	/* write not implemented */
	if (!filp->f_op->write)
		goto err;

	/* create missing block */
	ret = filp->f_op->write(filp, zero_buf, 1, &filp->f_pos);
	if (ret < 0)
		goto err;

	return 1;
err:
	printf("loop: cannot create missing block\n");
	return 0;
}

/*
 * Handle a read/write request.
 */
static void loop_request()
{
	uint32_t block, offset, real_block;
	size_t blocksize, len, size;
	struct request *request;
	struct buffer_head *bh;
	struct loop_device *lo;
	int block_present;
	char *dest_addr;

repeat:
	/* get next request */
	request = blk_dev[DEV_LOOP_MAJOR].current_request;
	if (!request)
		return;

	/* remove it from queue */
	blk_dev[DEV_LOOP_MAJOR].current_request = request->next;

	/* check device */
	if (minor(request->rq_dev) >= MAX_LOOP)
		goto err;

	/* get loop device */
	lo = &loop_dev[minor(request->rq_dev)];
	if (!lo->lo_dentry)
		goto err;

	/* get block size */
	blocksize = BLOCK_SIZE;
	if (blksize_size[major(lo->lo_device)] && blksize_size[major(lo->lo_device)][minor(lo->lo_device)])
		blocksize = blksize_size[major(lo->lo_device)][minor(lo->lo_device)];

	/* compute first block and offset */
	block = request->sector / (blocksize >> 9);
	offset = (request->sector % (blocksize >> 9)) << 9;
	len = request->nr_sectors << 9;
	dest_addr = request->buf;

	/* add loop offset */
	block += lo->lo_offset / blocksize;
	offset += lo->lo_offset % blocksize;

	/* fix offset */
	if (offset > blocksize) {
		block++;
		offset -= blocksize;
	}

	/* check command */
	if (request->cmd == WRITE) {
		if (lo->lo_flags & LO_FLAGS_READ_ONLY)
			goto err;
	} else if (request->cmd != READ) {
		goto err;
	}

	/* read block by block */
	while (len > 0) {
		real_block = block;
		block_present = 1;

		/* compute size to read */
		size = blocksize - offset;
		if (size > len)
			size = len;

		/* get real block */
		if (lo->lo_flags & LO_FLAGS_DO_BMAP) {
			real_block = generic_block_bmap(lo->lo_dentry->d_inode, block);
			if (!real_block) {
				if (request->cmd == READ) {
					memset(dest_addr, 0, size);
					block_present = 0;
				} else {
					if (!create_missing_block(lo, block, blocksize))
						goto err;

					real_block = generic_block_bmap(lo->lo_dentry->d_inode, block);
				}
			}
		}

		if (block_present) {
			/* get block */
			bh = getblk(lo->lo_device, real_block, blocksize);
			if (!bh)
				goto err;

			/* read it if needed */
			if (!buffer_uptodate(bh) && (request->cmd == READ || offset || len < blocksize)) {
				ll_rw_block(READ, 1, &bh);
				wait_on_buffer(bh);
				if (!buffer_uptodate(bh)) {
					brelse(bh);
					goto err;
				}
			}

			/* copy data */
			if (request->cmd == READ)
				memcpy(dest_addr, bh->b_data + offset, size);
			else
				memcpy(bh->b_data + offset, dest_addr, size);

			/* write : mark buffer up to date and dirty */
			if (request->cmd == WRITE) {
				mark_buffer_uptodate(bh, 1);
				mark_buffer_dirty(bh);
			}

			/* release block */
			brelse(bh);
		}

		/* go to next block */
		dest_addr += size;
		len -= size;
		offset = 0;
		block++;
	}

	/* end request */
	end_request(request);
	goto repeat;
err:
	printf("loop: error on request (cmd = 0x%x, sector = %ld)\n", request->cmd, request->sector);
	end_request(request);
	goto repeat;
}

/*
 * Attach a loop device.
 */
static int loop_set_fd(struct loop_device *lo, dev_t dev, unsigned long arg)
{
	struct inode *inode;
	struct file *filp;
	int ret;

	/* device already attached */
	ret = -EBUSY;
	if (lo->lo_dentry)
		goto out;

	/* get file */
	ret = -EBADF;
	filp = fget(arg);
	if (!filp)
		goto out;

	/* get inode */
	ret = -EINVAL;
	inode = filp->f_dentry->d_inode;
	if (!inode)
		goto out_fput;

	if (S_ISBLK(inode->i_mode)) {
		/* open block device */
		ret = blkdev_open(inode, filp);
		if (ret)
			goto out_fput;

		/* set loop device */
		lo->lo_device = inode->i_rdev;
		lo->lo_flags = 0;
		lo->lo_backing_file = NULL;
	} else if (S_ISREG(inode->i_mode)) {
		/* inode must implement bmap */
		if (!inode->i_op->bmap) {
			printf("loop: device must implement bmap\n");
			goto out_fput;
		}

		/* set device */
		lo->lo_device = inode->i_dev;
		lo->lo_flags = LO_FLAGS_DO_BMAP;

		/* get a new empty file */
		ret = -ENFILE;
		lo->lo_backing_file = get_empty_filp();
		if (!lo->lo_backing_file)
			goto out_fput;

		/* set backing file */
		lo->lo_backing_file->f_mode = filp->f_mode;
		lo->lo_backing_file->f_pos = filp->f_pos;
		lo->lo_backing_file->f_flags = filp->f_flags;
		lo->lo_backing_file->f_dentry = filp->f_dentry;
		lo->lo_backing_file->f_op = filp->f_op;
		lo->lo_backing_file->f_private = filp->f_private;
	}

	/* read only ? */
	if (IS_RDONLY(inode) || is_read_only(lo->lo_device)) {
		lo->lo_flags |= LO_FLAGS_READ_ONLY;
		set_device_ro(dev, 1);
	} else {
		invalidate_inode_pages(inode);
		set_device_ro(dev, 0);
	}

	/* attach file */
	lo->lo_dentry = dget(filp->f_dentry);
	figure_loop_size(lo);
	ret = 0;
out_fput:
	fput(filp);
out:
	return ret;
}

/*
 * Detach a loop device.
 */
static int loop_clr_fd(struct loop_device *lo)
{
	struct dentry *dentry = lo->lo_dentry;

	/* device not attached */
	if (!dentry)
		return -ENXIO;

	/* still busy */
	if (lo->lo_count > 1)
		return -EBUSY;

	/* release file */
	if (lo->lo_backing_file)
		fput(lo->lo_backing_file);
	else
		dput(dentry);

	/* reset devices */
	lo->lo_device = 0;
	lo->lo_dentry = NULL;
	lo->lo_backing_file = NULL;
	lo->lo_offset = 0;
	loop_sizes[lo->lo_number] = 0;

	return 0;
}

/*
 * Get loop device status.
 */
static int loop_get_status(struct loop_device *lo, struct loop_info64 *info)
{
	/* check argument */
	if (!info)
		return -EINVAL;

	/* device not attached */
	if (!lo->lo_dentry)
		return -ENXIO;

	/* get informations */
	memset(info, 0, sizeof(struct loop_info64));
	info->lo_number = lo->lo_number;
	info->lo_flags = lo->lo_flags;
	info->lo_device = lo->lo_dentry->d_inode->i_dev;
	info->lo_inode = lo->lo_dentry->d_inode->i_ino;
	info->lo_rdevice = lo->lo_device;
	info->lo_offset = lo->lo_offset;
	strncpy((char *) info->lo_file_name, lo->lo_name, LO_NAME_SIZE);

	return 0;
}

/*
 * Set loop device status.
 */
static int loop_set_status(struct loop_device *lo, struct loop_info64 *info)
{
	/* check argument */
	if (!info)
		return -EINVAL;

	/* set informations */
	lo->lo_offset = info->lo_offset;
	memcpy(lo->lo_name, info->lo_file_name, LO_NAME_SIZE);
	lo->lo_name[LO_NAME_SIZE - 1] = 0;

	return 0;
}

/*
 * Configure a loop device.
 */
static int loop_configure(struct loop_device *lo, dev_t dev, struct loop_config *config)
{
	int ret;

	/* set status */
	ret = loop_set_status(lo, &config->info);
	if (ret)
		return ret;

	/* attach device */
	return loop_set_fd(lo, dev, config->fd);
}

/*
 * Open a loop device.
 */
static int lo_open(struct inode *inode, struct file *filp)
{
	struct loop_device *lo;
	int dev;

	/* unused file */
	UNUSED(filp);

	/* check inode */
	if (!inode)
		return -EINVAL;

	/* check device */
	if (major(inode->i_rdev) != DEV_LOOP_MAJOR)
		return -ENODEV;

	/* check device */
	dev = minor(inode->i_rdev);
	if (dev >= MAX_LOOP)
		return -ENODEV;

	/* get device */
	lo = &loop_dev[dev];

	/* update reference count */
	lo->lo_count++;

	return 0;
}

/*
 * Close a loop device.
 */
static int lo_release(struct inode *inode, struct file *filp)
{
	struct loop_device *lo;
	int dev;

	/* unused file */
	UNUSED(filp);

	/* check inode */
	if (!inode)
		return 0;

	/* check device */
	if (major(inode->i_rdev) != DEV_LOOP_MAJOR)
		return 0;

	/* check device */
	dev = minor(inode->i_rdev);
	if (dev >= MAX_LOOP)
		return 0;

	/* get device */
	lo = &loop_dev[dev];

	/* synchronize device */
	sync_dev(inode->i_rdev);

	/* update reference count */
	if (lo->lo_count > 0)
		lo->lo_count--;

	return 0;
}

/*
 * Loop ioctl.
 */
static int lo_ioctl(struct inode *inode, struct file *filp, int request, unsigned long arg)
{
	struct loop_device *lo;
	int dev;

	/* unused file */
	UNUSED(filp);

	/* check inode */
	if (!inode)
		return 0;

	/* check device */
	if (major(inode->i_rdev) != DEV_LOOP_MAJOR)
		return 0;

	/* check device */
	dev = minor(inode->i_rdev);
	if (dev >= MAX_LOOP)
		return 0;

	/* get device */
	lo = &loop_dev[dev];

	/* ioctl */
	switch (request) {
		case LOOP_SET_FD:
			return loop_set_fd(lo, inode->i_rdev, arg);
		case LOOP_CLR_FD:
			return loop_clr_fd(lo);
		case LOOP_GET_STATUS64:
			return loop_get_status(lo, (struct loop_info64 *) arg);
		case LOOP_CONFIGURE:
			return loop_configure(lo, inode->i_rdev, (struct loop_config *) arg);
		default:
			printf("Unknown ioctl request (0x%x) on device 0x%x\n", request, (int) inode->i_rdev);
			return -EINVAL;
	}

	return 0;
}

/*
 * Loop file operations.
 */
static struct file_operations lo_fops = {
	.open		= lo_open,
	.release	= lo_release,
	.read		= generic_block_read,
	.write		= generic_block_write,
	.ioctl		= lo_ioctl,
};

/*
 * Init loop devices.
 */
int init_loop()
{
	int ret, i;

	/* register device */
	ret = register_blkdev(DEV_LOOP_MAJOR, "loop", &lo_fops);
	if (ret)
		return ret;

	/* set request function */
	blk_dev[DEV_LOOP_MAJOR].request = loop_request;

	/* init block size */
	memset(&loop_sizes, 0, sizeof(loop_sizes));
	memset(&loop_blksizes, 0, sizeof(loop_blksizes));
	blk_size[DEV_LOOP_MAJOR] = loop_sizes;
	blksize_size[DEV_LOOP_MAJOR] = loop_blksizes;

	/* init devices */
	for (i = 0; i < MAX_LOOP; i++) {
		memset(&loop_dev[i], 0, sizeof(struct loop_device));
		loop_dev[i].lo_number = i;
	}

	return 0;
}