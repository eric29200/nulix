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

#define PIPE_WPOS(inode)              ((inode)->i_zone[0])
#define PIPE_RPOS(inode)              ((inode)->i_zone[1])
#define PIPE_SIZE(inode)              ((PIPE_WPOS(inode) - PIPE_RPOS(inode)) & (PAGE_SIZE - 1))
#define PIPE_EMPTY(inode)             (PIPE_WPOS(inode) == PIPE_RPOS(inode))
#define PIPE_FULL(inode)              (PIPE_SIZE(inode) == (PAGE_SIZE - 1))

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
};

/*
 * In memory super block.
 */
struct super_block_t {
  uint16_t                    s_ninodes;
  uint16_t                    s_nzones;
  uint16_t                    s_imap_blocks;
  uint16_t                    s_zmap_blocks;
  uint16_t                    s_firstdatazone;
  uint16_t                    s_log_zone_size;
  uint32_t                    s_max_size;
  uint16_t                    s_magic;
  struct buffer_head_t *      s_imap[MINIX_IMAP_SLOTS];
  struct buffer_head_t *      s_zmap[MINIX_ZMAP_SLOTS];
  struct ata_device_t *       s_dev;
  struct inode_t *            s_imount;
  struct super_operations_t * s_op;
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
  ino_t                         i_ino;
  int                           i_ref;
  char                          i_dirt;
  char                          i_pipe;
  char                          i_rwait;
  char                          i_wwait;
  struct super_block_t *        i_sb;
  struct ata_device_t *         i_dev;
  struct inode_operations_t *   i_op;
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

/*
 * Super operations.
 */
struct super_operations_t {
  int (*read_inode)(struct inode_t *);
  int (*write_inode)(struct inode_t *);
  int (*put_inode)(struct inode_t *);
};

/*
 * Inode operations.
 */
struct inode_operations_t {
  int (*lookup)(struct inode_t *, const char *, size_t, struct inode_t **);
  int (*create)(struct inode_t *, const char *, size_t, mode_t, struct inode_t **);
  int (*link)(struct inode_t *, struct inode_t *, const char *, size_t);
  int (*unlink)(struct inode_t *, const char *, size_t);
  int (*symlink)(struct inode_t *, const char *, size_t, const char *);
  int (*mkdir)(struct inode_t *, const char *, size_t, mode_t);
  int (*rmdir)(struct inode_t *, const char *, size_t);
};

int minix_read_super(struct super_block_t *sb, struct ata_device_t *dev);
int minix_read_inode(struct inode_t *inode);
int minix_write_inode(struct inode_t *inode);
int minix_put_inode(struct inode_t *inode);
void minix_truncate(struct inode_t *inode);

int minix_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);
int minix_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode);
int minix_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len);
int minix_unlink(struct inode_t *dir, const char *name, size_t name_len);
int minix_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target);
int minix_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode);
int minix_rmdir(struct inode_t *dir, const char *name, size_t name_len);

/* file system operations */
int mount_root(struct ata_device_t *dev);

/* buffer operations */
struct buffer_head_t *bread(struct ata_device_t *dev, uint32_t block);
int bwrite(struct buffer_head_t *bh);
void brelse(struct buffer_head_t *bh);

/* inode operations */
struct inode_t *iget(struct super_block_t *sb, ino_t ino);
void iput(struct inode_t *inode);
struct inode_t *get_empty_inode();
struct inode_t *get_pipe_inode();
struct inode_t *new_inode(struct super_block_t *sb);
int free_inode(struct inode_t *inode);

/* block operations */
uint32_t new_block(struct super_block_t *sb);
int free_block(struct super_block_t *sb, uint32_t block);

/* name operations */
struct inode_t *namei(int dirfd, const char *pathname, int follow_links);
int open_namei(int dirfd, const char *pathname, int flags, mode_t mode, struct inode_t **res_inode);
int bmap(struct inode_t *inode, int block, int create);

/* read write operations */
int file_read(struct file_t *filp, char *buf, int count);
int file_write(struct file_t *filp, const char *buf, int count);
int read_char(dev_t dev, char *buf, int count);
int write_char(dev_t dev, const char *buf, int count);
int read_pipe(struct inode_t *inode, char *buf, int count);
int write_pipe(struct inode_t *inode, const char *buf, int count);

/* system calls */
int do_open(int dirfd, const char *pathname, int flags, mode_t mode);
int do_close(int fd);
ssize_t do_read(int fd, char *buf, int count);
ssize_t do_write(int fd, const char *buf, int count);
off_t do_lseek(int fd, off_t offset, int whence);
int do_stat(int dirfd, const char *filename, struct stat_t *statbuf);
int do_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx_t *statbuf);
int do_faccessat(int dirfd, const char *pathname, int flags);
int do_mkdir(int dirfd, const char *pathname, mode_t mode);
int do_link(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
ssize_t do_readlink(int dirfd, const char *pathname, char *buf, size_t bufsize);
int do_symlink(const char *target, int newdirfd, const char *linkpath);
int do_unlink(int dirfd, const char *pathname);
int do_rmdir(int dirfd, const char *pathname);
int do_getdents(int fd, struct dirent_t *dirent, uint32_t count);
int do_getdents64(int fd, void *dirp, size_t count);
int do_pipe(int pipefd[2]);

#endif
