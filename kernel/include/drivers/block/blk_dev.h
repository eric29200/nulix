#ifndef _BLK_DEV_H_
#define _BLK_DEV_H_

#include <fs/fs.h>
#include <dev.h>

#define MAX_BLKDEV		128

/*
 * Block device request.
 */
struct request {
	dev_t			dev;
	int			cmd;
	uint32_t		block;
	char *			buf;
	size_t			size;
};

/*
 * Block device.
 */
struct blk_dev {
	void (*request)(struct request *);
};

/* Block devices */
extern struct blk_dev blk_dev[MAX_BLKDEV];

void ll_rw_block(int rw, size_t nr_bhs, struct buffer_head *bhs[]);

#endif