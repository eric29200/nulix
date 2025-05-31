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
 * Execute requests.
 */
static void execute_requests(struct blk_dev *dev)
{
	size_t i;

	/* execute requests */
	for (i = 0; i < nr_requests; i++)
		dev->request(&requests[i]);
}

/*
 * Make a request.
 */
static void make_request(struct blk_dev *dev, int rw, struct buffer_head *bh)
{
	/* merge with previous request */
	if (nr_requests
		&& requests[nr_requests - 1].block_size == bh->b_size
		&& requests[nr_requests - 1].block + 1 == bh->b_block
		&& requests[nr_requests - 1].buf + bh->b_size == bh->b_data) {
		requests[nr_requests - 1].size += bh->b_size;
		return;
	}

	/* requests array full : execute requests */
	if (nr_requests == NR_REQUESTS) {
		execute_requests(dev);
		nr_requests = 0;
	}

	/* create new request */
	requests[nr_requests].dev = bh->b_dev;
	requests[nr_requests].cmd = rw;
	requests[nr_requests].block = bh->b_block;
	requests[nr_requests].block_size = bh->b_size;
	requests[nr_requests].buf = bh->b_data;
	requests[nr_requests].size = bh->b_size;
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
	for (i = 0, nr_requests = 0; i < nr_bhs; i++) {
		make_request(dev, rw, bhs[i]);
		mark_buffer_clean(bhs[i]);
		mark_buffer_uptodate(bhs[i], 1);
	}

	/* execute requests */
	if (nr_requests)
	 	execute_requests(dev);
}