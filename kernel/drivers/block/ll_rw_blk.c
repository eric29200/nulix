#include <drivers/block/blk_dev.h>
#include <proc/sched.h>
#include <stderr.h>
#include <stdio.h>

#define NR_REQUEST		128

/* block devices */
struct blk_dev blk_dev[MAX_BLKDEV];
static long ro_bits[MAX_BLKDEV][8] = { 0 };
size_t *blk_size[MAX_BLKDEV];
size_t *blksize_size[MAX_BLKDEV];

/* requests */
static struct request all_requests[NR_REQUEST];

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
void end_request(struct request *req)
{
	struct list_head *pos, *n;
	struct buffer_head *bh;

	/* end i/o */
	list_for_each_safe(pos, n, &req->bhs_list) {
		bh = list_entry(pos, struct buffer_head, b_list_req);
		bh->b_end_io(bh, 1);
	}

	/* mark request inactive */
	req->rq_status = RQ_INACTIVE;
}

/*
 * Execute requests.
 */
void execute_block_requests()
{
	size_t i;

	/* execute requests */
	for (i = 0; i < MAX_BLKDEV; i++)
		if (blk_dev[i].request && blk_dev[i].current_request)
			blk_dev[i].request();
}

/*
 * Get a free request.
 */
static struct request *get_request(dev_t dev)
{
	size_t i;

	for (i = 0; i < NR_REQUEST; i++)
		if (all_requests[i].rq_status == RQ_INACTIVE)
			goto found;

	return NULL;
found:
	all_requests[i].rq_status = RQ_ACTIVE;
	all_requests[i].rq_dev = dev;
	return &all_requests[i];
}

/*
 * Get a free request.
 */
static struct request *get_request_wait(dev_t dev)
{
	struct request *req;

	for (;;) {
		/* get a request */
		req = get_request(dev);
		if (req)
			return req;

		/* execute block requests */
		execute_block_requests();
	}
}

/*
 * Make a request.
 */
static void make_request(int rw, struct buffer_head *bh)
{
	struct request *req, *prev;
	struct blk_dev *bdev;
	uint32_t sector;
	size_t count;

	/* update i/o accounting */
	if (rw == READ)
		current_task->ioac.read_bytes += bh->b_size;
	else if (rw == WRITE)
		current_task->ioac.write_bytes += bh->b_size;

	/* lock buffer */
	lock_buffer(bh);

	/* get first sector and number of sectors */
	sector = bh->b_rsector;
	count = bh->b_size >> 9;

	/* get block device and current request */
	bdev = &blk_dev[major(bh->b_dev)];
	prev = bdev->current_request;

	/* merge with previous request */
	if (prev
		&& prev->cmd == rw
		&& prev->rq_dev == bh->b_dev
		&& prev->sector + prev->nr_sectors == sector
		&& prev->buf + (prev->nr_sectors << 9) == bh->b_data) {
		list_add_tail(&bh->b_list_req, &prev->bhs_list);
		prev->nr_sectors += count;
		return;
	}

	/* get a free request */
	req = get_request_wait(bh->b_dev);

	/* create new request */
	req->cmd = rw;
	req->rq_dev = bh->b_dev;
	req->sector = sector;
	req->nr_sectors = count;
	req->buf = bh->b_data;
	req->next = NULL;

	/* add buffer to request */
	INIT_LIST_HEAD(&req->bhs_list);
	list_add_tail(&bh->b_list_req, &req->bhs_list);

	/* add request to block device */
	if (bdev->current_request)
		req->next = bdev->current_request;
	bdev->current_request = req;
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

/*
 * Init block devices.
 */
void init_blk_dev()
{
	size_t i;

	/* init block devices */
	for (i = 0; i < MAX_BLKDEV; i++)
		blk_dev[i].current_request = NULL;

	/* init requests */
	for (i = 0; i < NR_REQUEST; i++)
		all_requests[i].rq_status = RQ_INACTIVE;
}