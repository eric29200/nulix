#ifndef _STAT_H_
#define _STAT_H_

#include <stddef.h>

#define STATX_BASIC_STATS	  0x000007FFU

/*
 * Stat file structure.
 */
struct stat_t {
  ino_t     st_ino;
  uint16_t  st_mode;
  uint8_t   st_nlinks;
  uid_t     st_uid;
  gid_t     st_gid;
  size_t    st_size;
  time_t    st_atime;
  time_t    st_mtime;
  time_t    st_ctime;
};

/*
 * Statx timestamp structure.
 */
struct statx_timestamp_t {
  int64_t   tv_sec;
  uint32_t  tv_nsec;
  int32_t   pad;
};

/*
 * Statx file structure.
 */
struct statx_t {
  uint32_t                  stx_mask;
  uint32_t                  stx_blksize;
  uint64_t                  stx_attributes;
  uint32_t                  stx_nlink;
  uint32_t                  stx_uid;
  uint32_t                  stx_gid;
  uint16_t                  stx_mode;
  uint16_t                  __spare0;
  uint64_t                  stx_ino;
  uint64_t                  stx_size;
  uint64_t                  stx_blocks;
  uint64_t                  stx_attributes_mask;
  struct statx_timestamp_t  stx_atime;
  struct statx_timestamp_t  stx_btime;
  struct statx_timestamp_t  stx_ctime;
  struct statx_timestamp_t  stx_mtime;
  uint32_t                  stx_rdev_major;
  uint32_t                  stx_rdev_minor;
  uint32_t                  stx_dev_major;
  uint32_t                  stx_dev_minor;
  uint64_t                  __spare1;
};


#endif
