#ifndef _FS_H_
#define _FS_H_

#include <lib/list.h>
#include <lib/htable.h>
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
#include <mm/mm.h>
#include <uio.h>
#include <time.h>

#define NR_INODE			4096
#define NR_FILE				256

#define MS_RDONLY			1

#define RENAME_NOREPLACE		(1 << 0)
#define RENAME_EXCHANGE			(1 << 1)
#define RENAME_WHITEOUT			(1 << 2)

#define DEFAULT_BLOCK_SIZE_BITS		10
#define DEFAULT_BLOCK_SIZE		(1 << DEFAULT_BLOCK_SIZE_BITS)

struct super_block_t;

/*
 * Buffer structure.
 */
struct buffer_head_t {
	uint32_t			b_block;		/* block number */
	char *				b_data;			/* data */
	size_t				b_size;			/* block size */
	int				b_ref;			/* reference counter */
	char				b_dirt;			/* dirty flag */
	char				b_uptodate;		/* up to date flag */
	dev_t				b_dev;			/* device number */
	struct buffer_head_t *		b_this_page;		/* next buffer in page */
	struct list_head_t		b_list;			/* next buffer in list */
	struct htable_link_t		b_htable;		/* buffer hash */
};

/*
 * File system structure.
 */
struct file_system_t {
	char *				name;
	int				requires_dev;
	int				(*read_super)(struct super_block_t *, void *, int);
	struct list_head_t		list;
};

/*
 * Generic super block.
 */
struct super_block_t {
	dev_t				s_dev;
	size_t				s_blocksize;
	uint8_t				s_blocksize_bits;
	void *				s_fs_info;
	uint16_t			s_magic;
	struct file_system_t *		s_type;
	struct inode_t *		s_root_inode;
	struct inode_t *		s_covered;
	struct super_operations_t *	s_op;
};

/*
 * Generic inode.
 */
struct inode_t {
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
	struct super_block_t *		i_sb;
	int				i_ref;
	char				i_dirt;
	struct inode_operations_t *	i_op;
	dev_t				i_rdev;
	char				i_pipe;
	char				i_shm;
	char				i_sock;
	struct inode_t *		i_mount;
	struct list_head_t		i_pages;
	struct list_head_t		i_mmap;
	struct list_head_t		i_list;
	struct htable_link_t		i_htable;
	union {
		struct minix_inode_info_t	minix_i;
		struct ext2_inode_info_t	ext2_i;
		struct pipe_inode_info_t	pipe_i;
		struct tmpfs_inode_info_t	tmp_i;
		struct isofs_inode_info_t	iso_i;
		struct socket_t			socket_i;
		void *				generic_i;
	} u;
};

/*
 * Opened file.
 */
struct file_t {
	uint16_t			f_mode;
	int				f_flags;
	size_t				f_pos;
	int				f_ref;
	struct inode_t *		f_inode;
	char *				f_path;
	void *				f_private;
	struct file_operations_t *	f_op;
};

/*
 * Directory entry (used by libc and getdents system call).
 */
struct dirent_t {
	ino_t				d_inode;
	off_t				d_off;
	unsigned short			d_reclen;
	unsigned char			d_type;
	char				d_name[];
};

/*
 * Directory entry (used by libc and getdents system call).
 */
struct dirent64_t {
	uint64_t			d_inode;
	int64_t				d_off;
	unsigned short			d_reclen;
	unsigned char			d_type;
	char				d_name[];
};

/*
 * Super operations.
 */
struct super_operations_t {
	void (*put_super)(struct super_block_t *);
	int (*read_inode)(struct inode_t *);
	int (*write_inode)(struct inode_t *);
	int (*put_inode)(struct inode_t *);
	void (*statfs)(struct super_block_t *, struct statfs64_t *);
};

/*
 * Inode operations.
 */
struct inode_operations_t {
	struct file_operations_t *fops;
	int (*lookup)(struct inode_t *, const char *, size_t, struct inode_t **);
	int (*create)(struct inode_t *, const char *, size_t, mode_t, struct inode_t **);
	int (*follow_link)(struct inode_t *, struct inode_t *, int, mode_t, struct inode_t **);
	ssize_t (*readlink)(struct inode_t *, char *, size_t);
	int (*link)(struct inode_t *, struct inode_t *, const char *, size_t);
	int (*unlink)(struct inode_t *, const char *, size_t);
	int (*symlink)(struct inode_t *, const char *, size_t, const char *);
	int (*mkdir)(struct inode_t *, const char *, size_t, mode_t);
	int (*rmdir)(struct inode_t *, const char *, size_t);
	int (*rename)(struct inode_t *, const char *, size_t, struct inode_t *, const char *, size_t);
	int (*mknod)(struct inode_t *, const char *, size_t, mode_t, dev_t);
	void (*truncate)(struct inode_t *);
	int (*bmap)(struct inode_t *, int);
	int (*readpage)(struct inode_t *, struct page_t *);
};

/*
 * File operations.
 */
struct file_operations_t {
	int (*open)(struct file_t *file);
	int (*close)(struct file_t *file);
	int (*read)(struct file_t *, char *, int);
	int (*write)(struct file_t *, const char *, int);
	int (*lseek)(struct file_t *, off_t, int);
	int (*getdents64)(struct file_t *, void *, size_t);
	int (*poll)(struct file_t *, struct select_table_t *);
	int (*ioctl)(struct file_t *, int, unsigned long);
	int (*mmap)(struct inode_t *, struct vm_area_t *);
};

/* files table */
extern struct file_t filp_table[NR_FILE];
extern struct inode_t *inode_table;

/* super operations */
int register_filesystem(struct file_system_t *fs);
struct file_system_t *get_filesystem(const char *name);
int get_filesystem_list(char *buf, int count);
int get_vfs_mount_list(char *buf, int count);

/* buffer operations */
struct buffer_head_t *bread(dev_t dev, uint32_t block, size_t blocksize);
int bwrite(struct buffer_head_t *bh);
void brelse(struct buffer_head_t *bh);
void bsync();
void bsync_dev(dev_t dev);
int binit();
struct buffer_head_t *getblk(dev_t dev, uint32_t block, size_t blocksize);
void try_to_free_buffer(struct buffer_head_t *bh);
void set_blocksize(dev_t dev, size_t blocksize);
int generic_block_read(struct file_t *filp, char *buf, int count);
int generic_block_write(struct file_t *filp, const char *buf, int count);
int generic_readpage(struct inode_t *inode, struct page_t *page);

/* inode operations */
struct inode_t *iget(struct super_block_t *sb, ino_t ino);
void iput(struct inode_t *inode);
struct inode_t *get_empty_inode(struct super_block_t *sb);
void clear_inode(struct inode_t *inode);
void insert_inode_hash(struct inode_t *inode);
struct inode_t *find_inode(struct super_block_t *sb, ino_t ino);
int iinit();

/* file operations */
struct file_t *get_empty_filp();

/* name operations */
struct inode_t *namei(int dirfd, struct inode_t *base, const char *pathname, int follow_links);
int open_namei(int dirfd, struct inode_t *base, const char *pathname, int flags, mode_t mode, struct inode_t **res_inode);

/* directory operations */
int filldir(struct dirent64_t *dirent, const char *name, size_t name_len, ino_t ino, size_t max_len);

/* character device driver */
struct inode_operations_t *char_get_driver(struct inode_t *inode);

/* block device driver */
struct inode_operations_t *block_get_driver(struct inode_t *inode);
int block_read(struct buffer_head_t *bh);
int block_write(struct buffer_head_t *bh);

/* filemap operations */
int generic_file_mmap(struct inode_t *inode, struct vm_area_t *vma);

/* generic operations */
int do_mount_root(dev_t dev, const char *dev_name);
int do_open(int dirfd, const char *pathname, int flags, mode_t mode);
int do_close(struct file_t *filp);
ssize_t do_read(struct file_t *filp, char *buf, int count);
ssize_t do_write(struct file_t *filp, const char *buf, int count);
off_t do_lseek(struct file_t *filp, off_t offset, int whence);
int do_truncate(struct inode_t *inode, off_t length);

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
int sys_utimensat(int dirfd, const char *pathname, struct timespec_t *times, int flags);
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
ssize_t sys_readv(int fd, const struct iovec_t *iov, int iovcnt);
ssize_t sys_writev(int fd, const struct iovec_t *iov, int iovcnt);
int sys_pread64(int fd, void *buf, size_t count, off_t offset);
int sys_copy_file_range(int fd_in, off_t *off_in, int fd_out, off_t *off_out, size_t len, unsigned int flags);
int sys_getdents64(int fd, void *dirp, size_t count);
int sys_poll(struct pollfd_t *fds, size_t nfds, int timeout);
int sys_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct old_timeval_t *timeout);
int sys_pselect6(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timespec_t *timeout, sigset_t *sigmask);
int sys_stat64(const char *pathname, struct stat64_t *statbuf);
int sys_lstat64(const char *pathname, struct stat64_t *statbuf);
int sys_fstat64(int fd, struct stat64_t *statbuf);
int sys_fstatat64(int dirfd, const char *pathname, struct stat64_t *statbuf, int flags);
int sys_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx_t *statbuf);
int sys_statfs64(const char *path, size_t size, struct statfs64_t *buf);
int sys_fstatfs64(int fd, struct statfs64_t *buf);
int sys_truncate64(const char *pathname, off_t length);
int sys_ftruncate64(int fd, off_t length);
int sys_sync();
int sys_fsync(int fd);

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
