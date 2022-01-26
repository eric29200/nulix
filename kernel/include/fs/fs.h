#ifndef _FS_H_
#define _FS_H_

#include <fs/stat.h>
#include <fs/poll.h>
#include <lib/htable.h>
#include <lib/list.h>
#include <time.h>
#include <dev.h>

#define NR_INODE                      256
#define NR_FILE                       256
#define BUFFER_HTABLE_BITS            12
#define NR_BUFFER                     (1 << BUFFER_HTABLE_BITS)

#define BLOCK_SIZE                    1024
#define BSYNC_TIMER_MS                10000

#define IMAP_SLOTS                    8
#define ZMAP_SLOTS                    64

#define PIPE_WPOS(inode)              ((inode)->i_zone[0])
#define PIPE_RPOS(inode)              ((inode)->i_zone[1])
#define PIPE_SIZE(inode)              ((PIPE_WPOS(inode) - PIPE_RPOS(inode)) & (PAGE_SIZE - 1))
#define PIPE_EMPTY(inode)             (PIPE_WPOS(inode) == PIPE_RPOS(inode))
#define PIPE_FULL(inode)              (PIPE_SIZE(inode) == (PAGE_SIZE - 1))

/*
 * Buffer structure.
 */
struct buffer_head_t {
  dev_t                   b_dev;
  char                    b_data[BLOCK_SIZE];
  int                     b_ref;
  char                    b_dirt;
  char                    b_uptodate;
  uint32_t                b_blocknr;
  struct list_head_t      b_list;
  struct htable_link_t    b_htable;
};

/*
 * Generic super block.
 */
struct super_block_t {
  uint32_t                    s_ninodes;
  uint32_t                    s_nzones;
  uint32_t                    s_imap_blocks;
  uint32_t                    s_zmap_blocks;
  uint32_t                    s_firstdatazone;
  uint32_t                    s_log_zone_size;
  uint32_t                    s_max_size;
  uint16_t                    s_magic;
  struct buffer_head_t *      s_imap[IMAP_SLOTS];
  struct buffer_head_t *      s_zmap[ZMAP_SLOTS];
  dev_t                       s_dev;
  struct inode_t *            s_covered;
  struct inode_t *            s_mounted;
  struct super_operations_t * s_op;
};

/*
 * Generic inode.
 */
struct inode_t {
  uint16_t                      i_mode;
  uid_t                         i_uid;
  uint32_t                      i_size;
  uint32_t                      i_time;
  gid_t                         i_gid;
  uint8_t                       i_nlinks;
  uint32_t                      i_zone[10];
  ino_t                         i_ino;
  int                           i_ref;
  char                          i_dirt;
  char                          i_pipe;
  char                          i_rwait;
  char                          i_wwait;
  struct super_block_t *        i_sb;
  dev_t                         i_dev;
  struct inode_t *              i_mount;
  struct inode_operations_t *   i_op;
};

/*
 * Opened file.
 */
struct file_t {
  uint16_t                  f_mode;
  int                       f_flags;
  size_t                    f_pos;
  int                       f_ref;
  struct inode_t *          f_inode;
  struct file_operations_t *f_op;
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
  struct file_operations_t *fops;
  int (*lookup)(struct inode_t *, const char *, size_t, struct inode_t **);
  int (*create)(struct inode_t *, const char *, size_t, mode_t, struct inode_t **);
  int (*follow_link)(struct inode_t *, struct inode_t **);
  ssize_t (*readlink)(struct inode_t *, char *, size_t);
  int (*link)(struct inode_t *, struct inode_t *, const char *, size_t);
  int (*unlink)(struct inode_t *, const char *, size_t);
  int (*symlink)(struct inode_t *, const char *, size_t, const char *);
  int (*mkdir)(struct inode_t *, const char *, size_t, mode_t);
  int (*rmdir)(struct inode_t *, const char *, size_t);
  int (*rename)(struct inode_t *, const char *, size_t, struct inode_t *, const char *, size_t);
  int (*mknod)(struct inode_t *, const char *, size_t, mode_t, dev_t);
  void (*truncate)(struct inode_t *);
};

/*
 * File operations.
 */
struct file_operations_t {
  int (*open)(struct file_t *file);
  int (*close)(struct file_t *file);
  int (*read)(struct file_t *, char *, int);
  int (*write)(struct file_t *, const char *, int);
  int (*getdents64)(struct file_t *, void *, size_t);
  int (*poll)(struct file_t *);
  int (*ioctl)(struct file_t *, int, unsigned long);
};

/* file system operations */
int mount_root(dev_t dev);

/* buffer operations */
struct buffer_head_t *bread(dev_t dev, uint32_t block);
void brelse(struct buffer_head_t *bh);
void bsync();
int binit();
struct buffer_head_t *getblk(dev_t dev, uint32_t block);

/* inode operations */
struct inode_t *iget(struct super_block_t *sb, ino_t ino);
void iput(struct inode_t *inode);
struct inode_t *get_empty_inode();

/* file operations */
struct file_t *get_empty_filp();

/* name operations */
struct inode_t *namei(int dirfd, const char *pathname, int follow_links);
int open_namei(int dirfd, const char *pathname, int flags, mode_t mode, struct inode_t **res_inode);

/* character device drivers */
struct inode_operations_t *char_get_driver(struct inode_t *inode);

/* system calls */
int do_mount(uint16_t magic, dev_t dev, const char *mount_point);
int do_open(int dirfd, const char *pathname, int flags, mode_t mode);
int do_close(int fd);
ssize_t do_read(int fd, char *buf, int count);
ssize_t do_write(int fd, const char *buf, int count);
off_t do_lseek(int fd, off_t offset, int whence);
int do_ioctl(int fd, int request, unsigned long arg);
int do_stat(int dirfd, const char *filename, struct stat_t *statbuf);
int do_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx_t *statbuf);
int do_faccessat(int dirfd, const char *pathname, int flags);
int do_mkdir(int dirfd, const char *pathname, mode_t mode);
int do_link(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
ssize_t do_readlink(int dirfd, const char *pathname, char *buf, size_t bufsize);
int do_symlink(const char *target, int newdirfd, const char *linkpath);
int do_unlink(int dirfd, const char *pathname);
int do_rmdir(int dirfd, const char *pathname);
int do_getdents64(int fd, void *dirp, size_t count);
int do_pipe(int pipefd[2]);
int do_rename(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int do_poll(struct pollfd_t *fds, size_t ndfs, int timeout);
int do_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timeval_t *timeout);
int do_chmod(const char *pathname, mode_t mode);
int do_fchmod(int fd, mode_t mode);
int do_mknod(int dirfd, const char *pathname, mode_t mode, dev_t dev);
int do_chown(const char *pathname, uid_t owner, gid_t group);
int do_fchown(int fd, uid_t owner, gid_t group);
int do_truncate(const char *pathname, off_t length);
int do_ftruncate(int fd, off_t length);
int do_utimensat(int dirfd, const char *pathname, const struct timespec_t times[2], int flags);

#endif
