#ifndef _BLK_DEV_H_
#define _BLK_DEV_H_

#include <fs/fs.h>
#include <ioctl.h>
#include <dev.h>

#define MAX_BLKDEV		128

#define BLKGETSIZE 		_IO(0x12, 96)
#define BLKBSZGET		_IOR(0x12, 112, sizeof(int))
#define BLKBSZSET		_IOW(0x12, 113, sizeof(int))
#define BLKGETSIZE64		_IOR(0x12, 114, sizeof(uint64_t))

/*
 * Block device request.
 */
struct request {
	dev_t			dev;
	int			cmd;
	uint32_t		block;
	size_t			block_size;
	char *			buf;
	size_t			size;
	size_t			nr_blocks;
	struct list_head	bhs_list;
};

/*
 * Block device.
 */
struct blk_dev {
	void (*request)(struct request *);
};

/* Block devices */
extern struct blk_dev blk_dev[MAX_BLKDEV];

int is_read_only(dev_t dev);
void set_device_ro(dev_t dev, int flag);
void ll_rw_block(int rw, size_t nr_bhs, struct buffer_head *bhs[]);
void execute_block_requests();

#endif
