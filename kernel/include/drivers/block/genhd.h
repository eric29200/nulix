#ifndef _GENHD_H_
#define _GENHD_H_

#include <stddef.h>
#include <lib/list.h>

#define PARTITION_MINOR_SHIFT		4
#define PARTITION_MINOR_MASK		((1 << PARTITION_MINOR_SHIFT) - 1)
#define NR_PARTITIONS			(1 << PARTITION_MINOR_SHIFT)

/*
 * Disk partition.
 */
struct partition {
	uint32_t		start_sect;			/* starting sector counting from 0 */
	uint32_t		nr_sects;			/* nr of sectors in partition */
};

/*
 * Disk partitions.
 */
struct gendisk {
	int 			major;				/* major number of driver */
	int			minor_shift;			/* number of times minor is shifted to get real minor */
	int 			max_p;				/* maximum partitions per device */
	struct partition *	part;				/* partitions */
	size_t *		sizes;				/* device size in blocks */
	int			nr_real;			/* number of real devices */
	struct list_head	list;				/* next gendisk */
};

/*
 * Msdos partition.
 */
struct msdos_partition {
	uint8_t			boot_ind;			/* 0x80 - active */
	uint8_t			head;				/* starting head */
	uint8_t			sector;				/* starting sector */
	uint8_t			cyl;				/* starting cylinder */
	uint8_t			sys_ind;			/* What partition type */
	uint8_t			end_head;			/* end head */
	uint8_t			end_sector;			/* end sector */
	uint8_t			end_cyl;			/* end cylinder */
	uint32_t		start_sect;			/* starting sector counting from 0 */
	uint32_t		nr_sects;			/* nr of sectors in partition */
};

void add_gendisk(struct gendisk *gd);
void setup_gendisk();

#endif
