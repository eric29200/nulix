#ifndef _MINIX_FS_H_
#define _MINIX_FS_H_

#include <stddef.h>

#define MINIX_V1			0x0001
#define MINIX_V2			0x0002
#define MINIX_V3			0x0003

#define MINIX1_MAGIC1			0x137F		/* Minix V1, 14 char names */
#define MINIX1_MAGIC2			0x138F		/* Minix V1, 30 char names */
#define MINIX2_MAGIC1			0x2468		/* Minix V2, 14 char names */
#define MINIX2_MAGIC2			0x2478		/* Minix V2, 14 char names */
#define MINIX3_MAGIC			0x4D5A		/* Minix V3, 60 char names */

#define MINIX_VALID_FS			0x0001
#define MINIX_ERROR_FS			0x0002

#define MINIX_ROOT_INODE		1

/*
 * Minix V3 super block.
 */
struct minix3_super_block_t {
	uint32_t		s_ninodes;
	uint16_t		s_pad0;
	uint16_t		s_imap_blocks;
	uint16_t		s_zmap_blocks;
	uint16_t		s_firstdatazone;
	uint16_t		s_log_zone_size;
	uint16_t		s_pad1;
	uint32_t		s_max_size;
	uint32_t		s_zones;
	uint16_t		s_magic;
	uint16_t		s_pad2;
	uint16_t		s_blocksize;
	uint8_t			s_disk_version;
};

/*
 * Minix in memory super block.
 */
struct minix_sb_info_t {
	uint32_t		s_ninodes;
	uint32_t		s_nzones;
	uint16_t		s_imap_blocks;
	uint16_t		s_zmap_blocks;
	uint16_t		s_firstdatazone;
	uint16_t		s_log_zone_size;
	uint16_t		s_version;
	uint16_t		s_state;
	int			s_name_len;
	int			s_dirsize;
	uint32_t		s_max_size;
	struct buffer_head_t *	s_sbh;
	struct buffer_head_t **	s_imap;
	struct buffer_head_t **	s_zmap;
};

/*
 * Minix V2/V3 inode.
 */
struct minix2_inode_t {
	uint16_t		i_mode;
	uint16_t		i_nlinks;
	uint16_t		i_uid;
	uint16_t		i_gid;
	uint32_t		i_size;
	uint32_t		i_atime;
	uint32_t		i_mtime;
	uint32_t		i_ctime;
	uint32_t		i_zone[10];
};

/*
 * Minix V3 directory entry.
 */
struct minix3_dir_entry_t {
	uint32_t		d_inode;
	char			d_name[0];
};

/*
 * Get minix in memory super block from generic super block.
 */
static inline struct minix_sb_info_t *minix_sb(struct super_block_t *sb)
{
	return sb->s_fs_info;
}

/* minix operations */
extern struct super_operations_t minix_sops;
extern struct inode_operations_t minix_file_iops;
extern struct inode_operations_t minix_dir_iops;
extern struct inode_operations_t minix_char_iops;
extern struct file_operations_t minix_file_fops;
extern struct file_operations_t minix_dir_fops;

/* minix super prototypes */
int init_minix_fs();

/* minix inode prototypes */
int minix_read_inode(struct inode_t *inode);
int minix_write_inode(struct inode_t *inode);
int minix_put_inode(struct inode_t *inode);
struct buffer_head_t *minix_getblk(struct inode_t *inode, int block, int create);
int minix_bmap(struct inode_t *inode, int block);

/* minix truncate prototypes */
void minix_truncate(struct inode_t *inode);

/* minix bitmap prototypes */
struct inode_t *minix_new_inode(struct super_block_t *sb);
int minix_free_inode(struct inode_t *inode);
uint32_t minix_count_free_inodes(struct super_block_t *sb);
uint32_t minix_new_block(struct super_block_t *sb);
int minix_free_block(struct super_block_t *sb, uint32_t block);
uint32_t minix_count_free_blocks(struct super_block_t *sb);

/* minix symlink prototypes */
int minix_follow_link(struct inode_t *dir, struct inode_t *inode, int flags, mode_t mode, struct inode_t **res_inode);
ssize_t minix_readlink(struct inode_t *inode, char *buf, size_t bufsize);

/* minix name resolutions prototypes */
int minix_lookup(struct inode_t *dir, const char *name, size_t name_len, struct inode_t **res_inode);
int minix_create(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, struct inode_t **res_inode);
int minix_link(struct inode_t *old_inode, struct inode_t *dir, const char *name, size_t name_len);
int minix_unlink(struct inode_t *dir, const char *name, size_t name_len);
int minix_symlink(struct inode_t *dir, const char *name, size_t name_len, const char *target);
int minix_mkdir(struct inode_t *dir, const char *name, size_t name_len, mode_t mode);
int minix_rmdir(struct inode_t *dir, const char *name, size_t name_len);
int minix_rename(struct inode_t *old_dir, const char *old_name, size_t old_name_len,
		 struct inode_t *new_dir, const char *new_name, size_t new_name_len);
int minix_mknod(struct inode_t *dir, const char *name, size_t name_len, mode_t mode, dev_t dev);

/* minix file prototypes */
int minix_file_read(struct file_t *filp, char *buf, int count);
int minix_file_write(struct file_t *filp, const char *buf, int count);
int minix_getdents64(struct file_t *filp, void *dirp, size_t count);

#endif
