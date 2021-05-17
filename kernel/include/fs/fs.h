#ifndef _FS_H_
#define _FS_H_

#include <fs/stat.h>
#include <drivers/ata.h>

#define NR_INODE                      256
#define NR_BUFFER                     256
#define NR_FILE                       256

#define MINIX_SUPER_MAGIC             0x138F
#define MINIX_ROOT_INODE              1
#define MINIX_IMAP_SLOTS              8
#define MINIX_ZMAP_SLOTS              8
#define MINIX_FILENAME_LEN            30
#define MINIX_INODES_PER_BLOCK        ((BLOCK_SIZE) / (sizeof(struct minix_inode_t)))
#define MINIX_DIR_ENTRIES_PER_BLOCK   ((BLOCK_SIZE) / (sizeof(struct minix_dir_entry_t)))

#define BLOCK_SIZE                    1024

/*
 * Buffer structure.
 */
struct buffer_head_t {
  struct ata_device_t *   b_dev;
  char                    b_data[BLOCK_SIZE];
  int                     b_ref;
  char                    b_dirt;
  uint32_t                b_blocknr;
};

/*
 * Minix super block.
 */
struct minix_super_block_t {
  uint16_t                s_ninodes;
  uint16_t                s_nzones;
  uint16_t                s_imap_blocks;
  uint16_t                s_zmap_blocks;
  uint16_t                s_firstdatazone;
  uint16_t                s_log_zone_size;
  uint32_t                s_max_size;
  uint16_t                s_magic;
  /* these are only in memory */
  struct buffer_head_t *  s_imap[MINIX_IMAP_SLOTS];
  struct buffer_head_t *  s_zmap[MINIX_ZMAP_SLOTS];
  struct ata_device_t *   s_dev;
  struct inode_t *        s_imount;
};

/*
 * Minix inode.
 */
struct minix_inode_t {
  uint16_t                i_mode;
  uint16_t                i_uid;
  uint32_t                i_size;
  uint32_t                i_time;
  uint8_t                 i_gid;
  uint8_t                 i_nlinks;
  uint16_t                i_zone[9];
};

/*
 * In memory inode.
 */
struct inode_t {
  uint16_t                      i_mode;
  uid_t                         i_uid;
  uint32_t                      i_size;
  uint32_t                      i_time;
  gid_t                         i_gid;
  uint8_t                       i_nlinks;
  uint16_t                      i_zone[9];
  /* these are only in memory */
  ino_t                         i_ino;
  int                           i_ref;
  char                          i_dirt;
  struct minix_super_block_t *  i_sb;
  struct ata_device_t *         i_dev;
};

/*
 * Opened file.
 */
struct file_t {
  uint16_t          f_mode;
  int               f_flags;
  size_t            f_pos;
  int               f_ref;
  struct inode_t *  f_inode;
};

/*
 * Minix dir entry.
 */
struct minix_dir_entry_t {
  uint16_t  inode;
  char      name[MINIX_FILENAME_LEN];
};

/*
 * Directory entry (used by libc and getdents system call).
 */
struct dirent_t {
  ino_t           d_inode;
  off_t           d_off;
  unsigned short  d_reclen;
  unsigned char   d_type;
  char            d_name[];
};

/*
 * Directory entry (used by libc and getdents system call).
 */
struct dirent64_t {
  uint64_t        d_inode;
  int64_t         d_off;
  unsigned short  d_reclen;
  unsigned char   d_type;
  char            d_name[];
};

/* file system operations */
int mount_root(struct ata_device_t *dev);

/* buffer operations */
struct buffer_head_t *bread(struct ata_device_t *dev, uint32_t block);
int bwrite(struct buffer_head_t *bh);
void brelse(struct buffer_head_t *bh);

/* inode operations */
struct inode_t *iget(struct minix_super_block_t *sb, ino_t ino);
struct inode_t *iget_parent(struct inode_t *dir);
void iput(struct inode_t *inode);
struct inode_t *get_empty_inode();
struct inode_t *new_inode();
int free_inode(struct inode_t *inode);
void truncate(struct inode_t *inode);

/* block operations */
uint32_t new_block();
int free_block(uint32_t block);

/* name operations */
struct inode_t *namei(const char *pathname, struct inode_t *start_inode);
int open_namei(const char *pathname, int flags, mode_t mode, struct inode_t **res_inode);
int bmap(struct inode_t *inode, int block, int create);

/* read write operations */
int file_read(struct inode_t *inode, struct file_t *filp, char *buf, int count);
int file_write(struct inode_t *inode, struct file_t *filp, const char *buf, int count);
int read_char(dev_t dev, char *buf, int count);
int write_char(dev_t dev, const char *buf, int count);

/* system calls */
int do_open(const char *pathname, int flags, mode_t mode);
int do_close(int fd);
ssize_t do_read(int fd, char *buf, int count);
ssize_t do_write(int fd, const char *buf, int count);
off_t do_lseek(int fd, off_t offset, int whence);
int do_stat(const char *filename, struct stat_t *statbuf);
int do_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx_t *statbuf);
int do_access(const char *filename);
int do_mkdir(const char *pathname, mode_t mode);
int do_link(const char *oldpath, const char *newpath);
int do_unlink(const char *pathname);
int do_rmdir(const char *pathname);
int do_getdents(int fd, struct dirent_t *dirent, uint32_t count);
int do_getdents64(int fd, void *dirp, size_t count);
int do_getcwd(char *buf, size_t size);

#endif
