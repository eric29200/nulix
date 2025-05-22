#include <drivers/block/blk_dev.h>
#include <stderr.h>
#include <stdio.h>

#define NR_REQUESTS		64

/* block devices */
struct blk_dev blk_dev[MAX_BLKDEV];

/* requests */
static struct request requests[NR_REQUESTS];
static size_t nr_requests = 0;

/*
 * Make a request.
 */
static void make_request(int rw, dev_t dev, uint32_t block, char *buf, size_t size)
{
	/* merge with previous request */
	if (nr_requests
		&& requests[nr_requests - 1].block + 1 == block
		&& requests[nr_requests - 1].buf + size == buf) {
		requests[nr_requests - 1].size += size;
		return;
	}

	/* create new request */
	requests[nr_requests].dev = dev;
	requests[nr_requests].cmd = rw;
	requests[nr_requests].block = block;
	requests[nr_requests].buf = buf;
	requests[nr_requests].size = size;
	nr_requests++;
}

/*
 * Read/write a block.
 */
void ll_rw_block(int rw, size_t nr_bhs, struct buffer_head *bhs[])
{
	uint32_t major, minor, correct_size, i;
	struct blk_dev *dev = NULL;

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

	/* make requests */
	nr_requests = 0;
	for (i = 0; i < nr_bhs; i++)
		make_request(rw, bhs[i]->b_dev, bhs[i]->b_block, bhs[i]->b_data, bhs[i]->b_size);

	/* execute requests */
	for (i = 0; i < nr_requests; i++)
		dev->request(&requests[i]);

	/* mark buffer clean and up to date */
	for (i = 0; i < nr_bhs; i++) {
		mark_buffer_clean(bhs[i]);
		mark_buffer_uptodate(bhs[i], 1);
	}
}