#include <drivers/block/genhd.h>
#include <drivers/block/blk_dev.h>
#include <fs/fs.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>
#include <dev.h>

/*
 * Add a partition.
 */
static void add_partition(struct gendisk *hd, int minor, uint32_t start, uint32_t size)
{
	hd->partitions[minor].start_sect = start;
	hd->partitions[minor].nr_sects = size;
}

/*
 * Discover msdos partitions.
 */
static int check_msdos_partition(struct gendisk *hd, dev_t dev)
{
	struct msdos_partition *partition;
	struct buffer_head *bh;
	int minor;

	/* reset partitions */
	memset(hd->partitions, 0, sizeof(struct partition) * NR_PARTITIONS);

	/* read partition table */
	bh = bread(dev, 0, 1024);
	if (!bh)
		goto out;

	/* check magic number */
	if (*((uint16_t *) (bh->b_data + 0x1FE)) != 0xAA55)
		goto out;

	/* get first partition */
	partition = (struct msdos_partition *) (bh->b_data + 0x1BE);

	/* check partitions */
	for (minor = 1; minor < 4; minor++, partition++) {
		/* empty partition */
		if (!partition->nr_sects)
			continue;

		/* add partition to disk */
		add_partition(hd, minor, partition->start_sect, partition->nr_sects);
	}

	brelse(bh);
	return 0;
 out:
	return -EINVAL;
}

/*
 * Discover partitions.
 */
void check_partition(struct gendisk *hd, dev_t dev)
{
	check_msdos_partition(hd, dev);
}
