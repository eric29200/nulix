#ifndef _FS_H_
#define _FS_H_

#include <stddef.h>
#include <drivers/ata.h>

#define MINIX_SUPER_MAGIC             0x138F
#define MINIX_I_MAP_SLOTS             8
#define MINIX_Z_MAP_SLOTS             8
#define MINIX_FILENAME_LEN            30
#define MINIX_INODES_PER_BLOCK        ((BLOCK_SIZE) / (sizeof(struct minix_inode_t)))
#define MINIX_DIR_ENTRIES_PER_BLOCK   ((BLOCK_SIZE) / (sizeof(struct minix_dir_entry_t)))

#define NR_OPEN                       32        /* maximum number of files opened by a process */

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
  struct ata_device_t *s_dev;
  struct inode_t *s_imount;
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
 * In memory inode.
 */
struct inode_t {
  uint16_t i_mode;
  uid_t i_uid;
  uint32_t i_size;
  uint32_t i_time;
  gid_t i_gid;
  uint8_t i_nlinks;
  uint16_t i_zone[9];
  ino_t i_ino;
  struct minix_super_block_t *i_sb;
  struct ata_device_t *i_dev;
};

/*
 * Opened file.
 */
struct file_t {
  uint16_t f_mode;
  off_t f_pos;
  struct inode_t *f_inode;
};

/*
 * Minix dir entry.
 */
struct minix_dir_entry_t {
  uint16_t inode;
  char name[MINIX_FILENAME_LEN];
};

/*
 * Stats file structure.
 */
struct stat_t {
  ino_t	st_ino;
  uint16_t st_mode;
  uint8_t st_nlinks;
  uid_t st_uid;
  gid_t st_gid;
  off_t st_size;
  time_t st_atime;
  time_t st_mtime;
  time_t st_ctime;
};

struct inode_t *read_inode(struct minix_super_block_t *sb, ino_t ino);
struct inode_t *namei(const char *pathname);
int open_namei(const char *pathname, struct inode_t **inode);

int bmap(struct inode_t *inode, int block);

int file_read(struct inode_t *inode, struct file_t *filp, char *buf, int count);

int mount_root(struct ata_device_t *dev);
int sys_open(const char *pathname);
int sys_close(int fd);
int sys_read(int fd, char *buf, int count);
int sys_stat(char *filename, struct stat_t *statbuf);

#endif