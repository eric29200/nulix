#ifndef _GENHD_H_
#define _GENHD_H_

#include <stddef.h>

#define PARTITION_MINOR_SHIFT		4
#define NR_PARTITIONS			(1 << PARTITION_MINOR_SHIFT)
#define DISK_NAME_LEN			32

/*
 * Disk partition.
 */
struct partition_t {
	uint32_t		start_sect;			/* starting sector counting from 0 */
	uint32_t		nr_sects;			/* nr of sectors in partition */
};

/*
 * Disk partitions.
 */
struct gendisk_t {
	dev_t			dev;				/* device number */
	char			name[DISK_NAME_LEN];		/* disk name */
	struct partition_t 	partitions[NR_PARTITIONS];	/* partitions */
};

/*
 * Msdos partition.
 */
struct msdos_partition_t {
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

void check_partition(struct gendisk_t *hd, dev_t dev);

#endif