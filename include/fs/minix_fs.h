#ifndef _FS_H_
#define _FS_H_

#include <stddef.h>
#include <drivers/ata.h>

#define MINIX_SUPER_MAGIC         0x138F
#define MINIX_I_MAP_SLOTS         8
#define MINIX_Z_MAP_SLOTS         8
#define MINIX_FILENAME_LEN        30

/*
 * Minix super block.
 */
struct minix_super_block_t {
  uint16_t s_ninodes;
  uint16_t s_nzones;
  uint16_t s_imap_blocks;
  uint16_t s_zmap_blocks;
  uint16_t s_firstdatazone;
  uint16_t s_log_zone_size;
  uint32_t s_max_size;
  uint16_t s_magic;
  /* these are only in memory */
  char **s_imap;
  char **s_zmap;
  struct ata_device_t *dev;
};

/*
 * Minix inode.
 */
struct minix_inode_t {
  uint16_t i_mode;
  uint16_t i_uid;
  uint32_t i_size;
  uint32_t i_time;
  uint8_t i_gid;
  uint8_t i_nlinks;
  uint16_t i_zone[9];
};

/*
 * Minix dir entry.
 */
struct minix_dir_entry_t {
  uint16_t inode;
  char name[MINIX_FILENAME_LEN];
};

int mount_root(struct ata_device_t *dev);

#endif
