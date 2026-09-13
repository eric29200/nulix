#include <drivers/block/genhd.h>
#include <drivers/block/blk_dev.h>
#include <fs/fs.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>
#include <dev.h>

/* gendisks list */
static LIST_HEAD(gendisks_list);

/*
 * Add a partition.
 */
static void add_partition(struct gendisk *gd, int minor, uint32_t start, uint32_t size)
{
	gd->part[minor].start_sect = start;
	gd->part[minor].nr_sects = size;
}

/*
 * Discover msdos partitions.
 */
static int check_msdos_partition(struct gendisk *gd, dev_t dev)
{
	struct msdos_partition *partition;
	struct buffer_head *bh;
	int minor;

	/* reset partitions */
	memset(gd->part, 0, sizeof(struct partition) * NR_PARTITIONS);

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
		add_partition(gd, minor, partition->start_sect, partition->nr_sects);
	}

	brelse(bh);
	return 0;
 out:
	return -EINVAL;
}

/*
 * Discover partitions.
 */
static void check_partition(struct gendisk *gd, dev_t dev)
{
	check_msdos_partition(gd, dev);
}

/*
 * Add a gendisk.
 */
void add_gendisk(struct gendisk *gd)
{
	list_add_tail(&gd->list, &gendisks_list);
}

/*
 * Setup a device.
 */
static void setup_dev(struct gendisk *gd)
{
	int end_minor = gd->nr_real * gd->max_p, drive, i;

	/* reset partitions size */
	blk_size[gd->major] = NULL;

	/* reset partitions */
	for (i = 0 ; i < end_minor; i++) {
		gd->part[i].start_sect = 0;
		gd->part[i].nr_sects = 0;
	}

	/* discover partitions */
	for (drive = 0 ; drive < gd->nr_real; drive++)
		check_partition(gd, mkdev(gd->major, drive << gd->minor_shift));

	/* set partitions size */
	if (gd->sizes != NULL) {
		for (i = 0; i < end_minor; i++)
			gd->sizes[i] = gd->part[i].nr_sects >> (BLOCK_SIZE_BITS - 9);
		blk_size[gd->major] = gd->sizes;
	}
}

/*
 * Setup gendisk = discover partitions.
 */
void setup_gendisk()
{
	struct list_head *pos;
	struct gendisk *gd;

	list_for_each(pos, &gendisks_list) {
		gd = list_entry(pos, struct gendisk, list);
		setup_dev(gd);
	}
}