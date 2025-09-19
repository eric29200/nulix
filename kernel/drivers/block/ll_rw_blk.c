#include <drivers/block/blk_dev.h>
#include <stderr.h>
#include <stdio.h>

#define NR_REQUESTS		64

/* block devices */
struct blk_dev blk_dev[MAX_BLKDEV];
static long ro_bits[MAX_BLKDEV][8] = { 0 };
size_t *blksize_size[MAX_BLKDEV];

/* requests */
static struct request requests[NR_REQUESTS];
static size_t nr_requests = 0;

/*
 * Is a device read only ?
 */
int is_read_only(dev_t dev)
{
	int major, minor;

	major = major(dev);
	minor = minor(dev);

	if (major < 0 || major >= MAX_BLKDEV)
		return 0;

	return ro_bits[major][minor >> 5] & (1 << (minor & 31));
}

/*
 * Set/Unset a device read only.
 */
void set_device_ro(dev_t dev, int flag)
{

	int major, minor;

	major = major(dev);
	minor = minor(dev);

	if (major < 0 || major >= MAX_BLKDEV)
		return;

	if (flag)
		ro_bits[major][minor >> 5] |= 1 << (minor & 31);
	else
		ro_bits[major][minor >> 5] &= ~(1 << (minor & 31));
}

/*
 * End a request.
 */
static void end_request(struct request *req)
{
	struct list_head *pos, *n;
	struct buffer_head *bh;

	list_for_each_safe(pos, n, &req->bhs_list) {
		bh = list_entry(pos, struct buffer_head, b_list_req);
		bh->b_end_io(bh, 1);
	}
}

/*
 * Execute requests.
 */
void execute_block_requests()
{
	size_t i;

	/* execute requests */
	for (i = 0; i < nr_requests; i++) {
		blk_dev[major(requests[i].dev)].request(&requests[i]);
		end_request(&requests[i]);
	}

	/* clear requests */
	nr_requests = 0;
}

/*
 * Make a request.
 */
static void make_request(int rw, struct buffer_head *bh)
{
	struct request *req, *prev = nr_requests ? &requests[nr_requests - 1] : NULL;
	uint32_t sector;
	size_t count;

	/* lock buffer */
	lock_buffer(bh);

	/* get first sector and number of sectors */
	sector = bh->b_rsector;
	count = bh->b_size >> 9;

	/* merge with previous request */
	if (prev
		&& prev->cmd == rw
		&& prev->dev == bh->b_dev
		&& prev->sector + prev->nr_sectors == sector
		&& prev->buf + (prev->nr_sectors << 9) == bh->b_data) {
		list_add_tail(&bh->b_list_req, &prev->bhs_list);
		prev->nr_sectors += count;
		return;
	}

	/* requests array full : execute requests */
	if (nr_requests == NR_REQUESTS)
		execute_block_requests();

	/* create new request */
	req = &requests[nr_requests++];
	req->cmd = rw;
	req->dev = bh->b_dev;
	req->sector = sector;
	req->nr_sectors = count;
	req->buf = bh->b_data;

	/* add buffer to request */
	INIT_LIST_HEAD(&req->bhs_list);
	list_add_tail(&bh->b_list_req, &req->bhs_list);
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
		goto err;
	}

	/* get correct size */
	correct_size = BLOCK_SIZE;
	if (blksize_size[major] && blksize_size[major][minor])
		correct_size = blksize_size[major][minor];

	/* check buffers size */
	for (i = 0; i < nr_bhs; i++) {
		if (bhs[i] && bhs[i]->b_size != correct_size) {
			printf("ll_rw_block: only %d blocks implemented\n", correct_size);
			goto err;
		}

		/* set real location */
		bhs[i]->b_rsector = bhs[i]->b_block * (bhs[i]->b_size >> 9);
	}

	/* read only device */
	if (rw == WRITE && is_read_only(bhs[0]->b_dev)) {
		printf("ll_rw_block: can't write to read only device 0x%x\n", bhs[0]->b_dev);
		goto err;
	}

	/* make requests */
	for (i = 0; i < nr_bhs; i++) {
		bhs[i]->b_count++;
		make_request(rw, bhs[i]);
	}

	return;
err:
	for (i = 0; i < nr_bhs; i++)
		mark_buffer_clean(bhs[i]);
}
