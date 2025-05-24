#ifndef _FS_H_
#define _FS_H_

#include <lib/list.h>
#include <fs/stat.h>
#include <fs/poll.h>
#include <fs/statfs.h>
#include <fs/minix_i.h>
#include <fs/ext2_i.h>
#include <fs/pipe_i.h>
#include <fs/tmp_i.h>
#include <fs/iso_i.h>
#include <net/socket.h>
#include <proc/wait.h>
#include <mm/paging.h>
#include <uio.h>
#include <time.h>
#include <string.h>

#define NR_INODE			4096
#define NR_FILE				256

#define MS_RDONLY			1

#define RENAME_NOREPLACE		(1 << 0)
#define RENAME_EXCHANGE			(1 << 1)
#define RENAME_WHITEOUT			(1 << 2)

#define DEFAULT_BLOCK_SIZE_BITS		10
#define DEFAULT_BLOCK_SIZE		(1 << DEFAULT_BLOCK_SIZE_BITS)

#define READ				0
#define WRITE				1

struct super_block;

/*
 * Buffer structure.
 */
struct buffer_head {
	uint32_t			b_block;		/* block number */
	char *				b_data;			/* data */
	size_t				b_size;			/* block size */
	int				b_count;			/* reference counter */
	char				b_dirt;			/* dirty flag */
	char				b_uptodate;		/* up to date flag */
	dev_t				b_dev;			/* device number */
	struct buffer_head *		b_this_page;		/* next buffer in page */
	struct list_head		b_list;			/* next buffer in list */
	struct buffer_head *		b_next_hash;		/* next buffer in hash list */
	struct buffer_head *		b_prev_hash;		/* previous buffer in hash list */
};

/*
 * File system structure.
 */
struct file_system {
	char *				name;
	int				requires_dev;
	int				(*read_super)(struct super_block *, void *, int);
	struct list_head		list;
};

/*
 * Generic super block.
 */
struct super_block {
	dev_t				s_dev;
	size_t				s_blocksize;
	uint8_t				s_blocksize_bits;
	void *				s_fs_info;
	uint16_t			s_magic;
	struct file_system *		s_type;
	struct inode *			s_root_inode;
	struct inode *			s_covered;
	struct super_operations *	s_op;
};

/*
 * Generic inode.
 */
struct inode {
	uint16_t			i_mode;
	uint8_t				i_nlinks;
	uid_t				i_uid;
	gid_t				i_gid;
	uint32_t			i_size;
	uint32_t			i_blocks;
	time_t				i_atime;
	time_t				i_mtime;
	time_t				i_ctime;
	ino_t				i_ino;
	struct super_block *		i_sb;
	int				i_count;
	char				i_dirt;
	struct inode_operations *	i_op;
	dev_t				i_rdev;
	char				i_pipe;
	char				i_shm;
	char				i_sock;
	struct inode *			i_mount;
	struct list_head		i_pages;
	struct list_head		i_mmap;
	struct list_head		i_list;
	struct inode *			i_next_hash;
	struct inode *			i_prev_hash;
	union {
		struct minix_inode_info		minix_i;
		struct ext2_inode_info		ext2_i;
		struct pipe_inode_info		pipe_i;
		struct tmpfs_inode_info		tmp_i;
		struct isofs_inode_info		iso_i;
		struct socket			socket_i;
		void *				generic_i;
	} u;
};

/*
 * Opened file.
 */
struct file {
	uint16_t			f_mode;
	int				f_flags;
	size_t				f_pos;
	int				f_count;
	struct inode *			f_inode;
	char *				f_path;
	void *				f_private;
	struct file_operations *	f_op;
};

/*
 * Directory entry (used by libc and getdents system call).
 */
struct dirent {
	ino_t				d_inode;
	off_t				d_off;
	unsigned short			d_reclen;
	unsigned char			d_type;
	char				d_name[];
};

/*
 * Directory entry (used by libc and getdents system call).
 */
struct dirent64 {
	uint64_t			d_inode;
	int64_t				d_off;
	unsigned short			d_reclen;
	unsigned char			d_type;
	char				d_name[];
};

/*
 * Super operations.
 */
struct super_operations {
	void (*put_super)(struct super_block *);
	int (*read_inode)(struct inode *);
	int (*write_inode)(struct inode *);
	int (*put_inode)(struct inode *);
	void (*statfs)(struct super_block *, struct statfs64 *);
};

/*
 * Inode operations.
 */
struct inode_operations {
	struct file_operations *fops;
	int (*lookup)(struct inode *, const char *, size_t, struct inode **);
	int (*create)(struct inode *, const char *, size_t, mode_t, struct inode **);
	int (*follow_link)(struct inode *, struct inode *, int, mode_t, struct inode **);
	ssize_t (*readlink)(struct inode *, char *, size_t);
	int (*link)(struct inode *, struct inode *, const char *, size_t);
	int (*unlink)(struct inode *, const char *, size_t);
	int (*symlink)(struct inode *, const char *, size_t, const char *);
	int (*mkdir)(struct inode *, const char *, size_t, mode_t);
	int (*rmdir)(struct inode *, const char *, size_t);
	int (*rename)(struct inode *, const char *, size_t, struct inode *, const char *, size_t);
	int (*mknod)(struct inode *, const char *, size_t, mode_t, dev_t);
	void (*truncate)(struct inode *);
	int (*bmap)(struct inode *, int);
	int (*readpage)(struct inode *, struct page *);
};

/*
 * File operations.
 */
struct file_operations {
	int (*open)(struct file *file);
	int (*close)(struct file *file);
	int (*read)(struct file *, char *, int);
	int (*write)(struct file *, const char *, int);
	int (*lseek)(struct file *, off_t, int);
	int (*getdents64)(struct file *, void *, size_t);
	int (*poll)(struct file *, struct select_table *);
	int (*ioctl)(struct file *, int, unsigned long);
	int (*mmap)(struct inode *, struct vm_area *);
};

/* files table */
extern struct file filp_table[NR_FILE];
extern uint32_t buffermem_pages;

/* super operations */
int register_filesystem(struct file_system *fs);
struct file_system *get_filesystem(const char *name);
int get_filesystem_list(char *buf, int count);
int get_vfs_mount_list(char *buf, int count);
int fs_may_umount(struct super_block *sb);

/* buffer operations */
int buffer_dirty(struct buffer_head *bh);
int buffer_uptodate(struct buffer_head *bh);
void mark_buffer_clean(struct buffer_head *bh);
void mark_buffer_dirty(struct buffer_head *bh);
void mark_buffer_uptodate(struct buffer_head *bh, int on);
struct buffer_head *bread(dev_t dev, uint32_t block, size_t blocksize);
void brelse(struct buffer_head *bh);
void bsync();
void bsync_dev(dev_t dev);
void init_buffer();
struct buffer_head *getblk(dev_t dev, uint32_t block, size_t blocksize);
void try_to_free_buffer(struct buffer_head *bh);
void set_blocksize(dev_t dev, size_t blocksize);
int generic_block_read(struct file *filp, char *buf, int count);
int generic_block_write(struct file *filp, const char *buf, int count);
int generic_block_ioctl(struct file *filp, int request, unsigned long arg);
int generic_readpage(struct inode *inode, struct page *page);
int generic_file_read(struct file *filp, char *buf, int count);

/* inode operations */
struct inode *iget(struct super_block *sb, ino_t ino);
void iput(struct inode *inode);
struct inode *get_empty_inode(struct super_block *sb);
void clear_inode(struct inode *inode);
void add_to_inode_cache(struct inode *inode);
struct inode *find_inode(struct super_block *sb, ino_t ino);
void init_inode();

/* file operations */
struct file *get_empty_filp();

/* name operations */
int namei(int dirfd, struct inode *base, const char *pathname, int follow_links, struct inode **res_inode);
int open_namei(int dirfd, struct inode *base, const char *pathname, int flags, mode_t mode, struct inode **res_inode);

/* directory operations */
int filldir(struct dirent64 *dirent, const char *name, size_t name_len, ino_t ino, size_t max_len);

/* character device driver */
struct inode_operations *char_get_driver(struct inode *inode);

/* block device driver */
struct inode_operations *block_get_driver(struct inode *inode);

/* filemap operations */
int generic_file_mmap(struct inode *inode, struct vm_area *vma);

/* generic operations */
int do_mount_root(dev_t dev, const char *dev_name);
int do_open(int dirfd, const char *pathname, int flags, mode_t mode);
int do_close(struct file *filp);
ssize_t do_read(struct file *filp, char *buf, int count);
ssize_t do_write(struct file *filp, const char *buf, int count);
off_t do_lseek(struct file *filp, off_t offset, int whence);
int do_truncate(struct inode *inode, off_t length);

/* system calls */
int sys_access(const char *filename, mode_t mode);
int sys_faccessat(int dirfd, const char *pathname, int flags);
int sys_faccessat2(int dirfd, const char *pathname, int flags);
int sys_fadvise64(int fd, off_t offset, off_t len, int advice);
int sys_mount(char *dev_name, char *dir_name, char *type, unsigned long flags, void *data);
int sys_umount2(const char *target, int flags);
int sys_open(const char *pathname, int flags, mode_t mode);
int sys_openat(int dirfd, const char *pathname, int flags, mode_t mode);
int sys_creat(const char *pathname, mode_t mode);
int sys_close(int fd);
int sys_chmod(const char *pathname, mode_t mode);
int sys_fchmod(int fd, mode_t mode);
int sys_fchmodat(int dirfd, const char *pathname, mode_t mode, unsigned int flags);
int sys_chown(const char *pathname, uid_t owner, gid_t group);
int sys_fchown(int fd, uid_t owner, gid_t group);
int sys_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, unsigned int flags);
int sys_utimensat(int dirfd, const char *pathname, struct timespec *times, int flags);
int sys_chroot(const char *path);
int sys_chdir(const char *path);
int sys_fchdir(int fd);
int sys_getcwd(char *buf, size_t size);
int sys_dup(int oldfd);
int sys_dup2(int oldfd, int newfd);
int sys_fcntl(int fd, int cmd, unsigned long arg);
int sys_ioctl(int fd, unsigned long request, unsigned long arg);
int sys_mkdir(const char *filename, mode_t mode);
int sys_mkdirat(int dirfd, const char *pathname, mode_t mode);
int sys_link(const char *oldpath, const char *newpath);
int sys_linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int sys_symlink(const char *target, const char *linkpath);
int sys_symlinkat(const char *target, int newdirfd, const char *linkpath);
int sys_unlink(const char *pathname);
int sys_unlinkat(int dirfd, const char *pathname, int flags);
int sys_rmdir(const char *pathname);
ssize_t sys_readlink(const char *pathname, char *buf, size_t bufsize);
ssize_t sys_readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsize);
int sys_rename(const char *oldpath, const char *newpath);
int sys_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int sys_renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags);
int sys_mknod(const char *pathname, mode_t mode, dev_t dev);
int sys_pipe(int pipefd[2]);
int sys_pipe2(int pipefd[2], int flags);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_llseek(int fd, uint32_t offset_high, uint32_t offset_low, off_t *result, int whence);
int sys_read(int fd, char *buf, int count);
int sys_write(int fd, const char *buf, int count);
ssize_t sys_readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t sys_writev(int fd, const struct iovec *iov, int iovcnt);
int sys_pread64(int fd, void *buf, size_t count, off_t offset);
int sys_copy_file_range(int fd_in, off_t *off_in, int fd_out, off_t *off_out, size_t len, unsigned int flags);
int sys_getdents64(int fd, void *dirp, size_t count);
int sys_poll(struct pollfd *fds, size_t nfds, int timeout);
int sys_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct old_timeval *timeout);
int sys_pselect6(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct old_timeval *timeout, sigset_t *sigmask);
int sys_stat64(const char *pathname, struct stat64 *statbuf);
int sys_lstat64(const char *pathname, struct stat64 *statbuf);
int sys_fstat64(int fd, struct stat64 *statbuf);
int sys_fstatat64(int dirfd, const char *pathname, struct stat64 *statbuf, int flags);
int sys_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statbuf);
int sys_statfs64(const char *path, size_t size, struct statfs64 *buf);
int sys_fstatfs64(int fd, struct statfs64 *buf);
int sys_truncate64(const char *pathname, off_t length);
int sys_ftruncate64(int fd, off_t length);
int sys_sync();
int sys_fsync(int fd);
ssize_t sys_sendfile64(int fd_out, int fd_in, off_t *offset, size_t count);

/*
 * Compute block size in bits from block in size in byte.
 */
static inline uint32_t blksize_bits(uint32_t size)
{
	uint32_t bits = 8;

	do {
		bits++;
		size >>= 1;
	} while (size > 256);

	return bits;
}

#endif
