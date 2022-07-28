#ifndef _STAT_H_
#define _STAT_H_

#include <stddef.h>

#define STATX_BASIC_STATS	0x000007FFU

/*
 * Stat64 structure.
 */
struct stat64_t {
	unsigned long long	st_dev;
	unsigned char		__pad0[4];
	unsigned long		__st_ino;
	unsigned int		st_mode;
	unsigned int		st_nlink;
	unsigned long		st_uid;
	unsigned long		st_gid;
	unsigned long long	st_rdev;
	unsigned char		__pad3[4];
	long long		st_size;
	unsigned long		st_blksize;
	unsigned long long	st_blocks;
	unsigned long		st_atime;
	unsigned long		st_atime_nsec;
	unsigned long		st_mtime;
	unsigned int		st_mtime_nsec;
	unsigned long		st_ctime;
	unsigned long		st_ctime_nsec;
	unsigned long long	st_ino;
};

/*
 * Statx timestamp structure.
 */
struct statx_timestamp_t {
	int64_t		tv_sec;
	uint32_t	tv_nsec;
	int32_t		pad;
};

/*
 * Statx file structure.
 */
struct statx_t {
	uint32_t			stx_mask;
	uint32_t			stx_blksize;
	uint64_t			stx_attributes;
	uint32_t			stx_nlink;
	uint32_t			stx_uid;
	uint32_t			stx_gid;
	uint16_t			stx_mode;
	uint16_t			__spare0;
	uint64_t			stx_ino;
	uint64_t			stx_size;
	uint64_t			stx_blocks;
	uint64_t			stx_attributes_mask;
	struct statx_timestamp_t	stx_atime;
	struct statx_timestamp_t	stx_btime;
	struct statx_timestamp_t	stx_ctime;
	struct statx_timestamp_t	stx_mtime;
	uint32_t			stx_rdev_major;
	uint32_t			stx_rdev_minor;
	uint32_t			stx_dev_major;
	uint32_t			stx_dev_minor;
	uint64_t			__spare1;
};

#endif
