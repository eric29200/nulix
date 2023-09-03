#ifndef _STATFS_H_
#define _STATFS_H_

#include <stddef.h>

/*
 * File systems statistics.
 */
struct statfs64 {
	uint32_t	f_type;
	uint32_t	f_bsize;
	uint64_t	f_blocks;
	uint64_t	f_bfree;
	uint64_t	f_bavail;
	uint64_t	f_files;
	uint64_t	f_ffree;
	uint64_t	f_fsid;
	uint32_t	f_namelen;
	uint32_t	f_frsize;
	uint32_t	f_flags;
	uint32_t	f_spare[4];
};

#endif
