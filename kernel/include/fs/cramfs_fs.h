#ifndef _CRAMFS_FS_H_
#define _CRAMFS_FS_H_

#include <stddef.h>
#include <fcntl.h>

#define CRAMFS_MAGIC		0x28CD3D45
#define CRAMFS_SIGNATURE	"Compressed ROMFS"
#define CRAMFS_SUPPORTED_FLAGS	0xFF


#define CRAMFS_MODE_WIDTH	16
#define CRAMFS_UID_WIDTH	16
#define CRAMFS_SIZE_WIDTH	24
#define CRAMFS_GID_WIDTH	8
#define CRAMFS_NAMELEN_WIDTH	6
#define CRAMFS_OFFSET_WIDTH	26

#define CRAMOFFSET(inode)	((inode)->i_ino)

/*
 * Cramfs file system informations.
 */
struct cramfs_info {
	uint32_t		crc;
	uint32_t		edition;
	uint32_t		blocks;
	uint32_t		files;
};

/*
 * Cramfs inode.
 */
struct cramfs_inode {
	uint32_t 		mode:CRAMFS_MODE_WIDTH;
	uint32_t		uid:CRAMFS_UID_WIDTH;
	uint32_t		size:CRAMFS_SIZE_WIDTH;
	uint32_t 		gid:CRAMFS_GID_WIDTH;
	uint32_t		namelen:CRAMFS_NAMELEN_WIDTH;
	uint32_t		offset:CRAMFS_OFFSET_WIDTH;
};

/*
 * Cramfs super block.
 */
struct cramfs_super_block {
	uint32_t		magic;
	uint32_t		size;
	uint32_t		flags;
	uint32_t		future;
	uint8_t			signature[16];
	struct cramfs_info	fsid;
	uint8_t			name[16];
	struct cramfs_inode	root;
};

/* cramfs operations */
extern struct super_operations cramfs_sops;
extern struct inode_operations cramfs_dir_iops;

/* cramfs super prototypes */
int init_cramfs_fs();
void *cramfs_read(struct super_block *sb, uint32_t offset, size_t len);

/* cramfs inode prototypes */
struct inode *cramfs_get_inode(struct super_block *sb, struct cramfs_inode *cramfs_inode, uint32_t offset);
int cramfs_uncompress_block(void *src, int src_len, void *dst, int dst_len);

/* cramfs name resolutions prototypes */
struct dentry *cramfs_lookup(struct inode *dir, struct dentry *dentry);

/* cramfs read directory prototypes */
int cramfs_readdir(struct file *filp, void *dirent, filldir_t filldir);

/*
 * Get inode number of a cramfs inode.
 */
static inline ino_t cramino(const struct cramfs_inode *cino, uint32_t offset)
{
	if (!cino->offset)
		return offset + 1;
	if (!cino->size)
		return offset + 1;

	switch (cino->mode & S_IFMT) {
		case S_IFREG:
		case S_IFDIR:
		case S_IFLNK:
			return cino->offset << 2;
		default:
			break;
	}

	return offset + 1;
}

#endif