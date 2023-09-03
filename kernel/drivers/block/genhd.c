#include <drivers/block/genhd.h>
#include <fs/dev_fs.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>
#include <dev.h>

/*
 * Add a partition.
 */
static int add_partition(struct gendisk *hd, int i, uint32_t start_sect, uint32_t nr_sects)
{
	char partition_name[DISK_NAME_LEN];

	/* set partition */
	hd->partitions[i].start_sect = start_sect;
	hd->partitions[i].nr_sects = nr_sects;

	/* set partition name */
	sprintf(partition_name, "%s%d", hd->name, i);

	/* register partition in devfs */
	if (!devfs_register(NULL, partition_name, S_IFBLK | 0660, hd->dev + i))
		return -ENOSPC;

	/* set block size */
	blocksize_size[major(hd->dev)][minor(hd->dev) + i] = DEFAULT_BLOCK_SIZE;

	return 0;
}

/*
 * Discover msdos partitions.
 */
static int check_msdos_partition(struct gendisk *hd, dev_t dev)
{
	struct msdos_partition *partition;
	struct buffer_head *bh;
	int i;

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
	for (i = 1; i < 4; i++, partition++) {
		/* empty partition */
		if (!partition->nr_sects)
			continue;

		/* add partition to disk */
		if (add_partition(hd, i, partition->start_sect, partition->nr_sects))
			printf("[Kernel] Can't register partition %d of disk %x", i, hd->dev);
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
