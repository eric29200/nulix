#include <drivers/block/blk_dev.h>
#include <stderr.h>
#include <stdio.h>

/* block devices */
struct blk_dev blk_dev[MAX_BLKDEV];

/*
 * Read/write a block.
 */
void ll_rw_block(int rw, size_t nr_bhs, struct buffer_head *bhs[])
{
	uint32_t major, minor, correct_size, i;
	struct blk_dev *dev = NULL;
	struct request request;

	/* get block device */
	major = major(bhs[0]->b_dev);
	minor = minor(bhs[0]->b_dev);
	if (major < MAX_BLKDEV)
		dev = &blk_dev[major];

	/* check device */
	if (!dev || !dev->request) {
		printf("ll_rw_block: trying to read non existent block device 0x%x\n", bhs[0]->b_dev);
		return;
	}

	/* get correct size */
	correct_size = DEFAULT_BLOCK_SIZE;
	if (blocksize_size[major]) {
		i = blocksize_size[major][minor];
		if (i)
			correct_size = i;
	}

	/* check buffers size */
	for (i = 0; i < nr_bhs; i++) {
		if (bhs[i] && bhs[i]->b_size != correct_size) {
			printf("ll_rw_block: only %d blocks implemented\n", correct_size);
			return;
		}
	}

	/* make request */
	request.dev = bhs[0]->b_dev;
	request.cmd = rw;
	request.bhs = bhs;
	request.nr_bhs = nr_bhs;
	dev->request(&request);
}