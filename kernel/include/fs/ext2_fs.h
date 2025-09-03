#ifndef _EXT2_FS_H_
#define _EXT2_FS_H_

#include <stddef.h>

#define EXT2_SUPER_MAGIC		0xEF53
#define EXT2_ROOT_INO			2
#define EXT2_NAME_LEN			255

/*
 * Constants relative to the data blocks
 */
#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

/*
 * Revision levels
 */
#define EXT2_GOOD_OLD_REV		0	/* The good old (original) format */
#define EXT2_DYNAMIC_REV		1 	/* V2 format w/ dynamic inode sizes */
#define EXT2_GOOD_OLD_INODE_SIZE 	128
#define EXT2_GOOD_OLD_FIRST_INO		11

#define EXT2_INODE_SIZE(sb)		(ext2_sb(sb)->s_inode_size)

#define EXT2_DIR_PAD			4
#define EXT2_DIR_ROUND			(EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT2_DIR_ROUND) & ~EXT2_DIR_ROUND)

#define EXT2_BITMAP_SET(bh, i)		((bh)->b_data[(i) / 8] |= (0x1 << ((i) % 8)))
#define EXT2_BITMAP_CLR(bh, i)		((bh)->b_data[(i) / 8] &= ~(0x1 << ((i) % 8)))


/*
 * Ext2 super block.
 */
struct ext2_super_block {
	uint32_t	s_inodes_count;					/* Inodes count */
	uint32_t	s_blocks_count;					/* Blocks count */
	uint32_t	s_r_blocks_count;				/* Reserved blocks count */
	uint32_t	s_free_blocks_count;				/* Free blocks count */
	uint32_t	s_free_inodes_count;				/* Free inodes count */
	uint32_t	s_first_data_block;			 	/* First Data Block */
	uint32_t	s_log_block_size;				/* Block size */
	uint32_t	s_log_frag_size;				/* Fragment size */
	uint32_t	s_blocks_per_group;			 	/* # Blocks per group */
	uint32_t	s_frags_per_group;				/* # Fragments per group */
	uint32_t	s_inodes_per_group;			 	/* # Inodes per group */
	uint32_t	s_mtime;					/* Mount time */
	uint32_t	s_wtime;					/* Write time */
	uint16_t	s_mnt_count;					/* Mount count */
	uint16_t	s_max_mnt_count;				/* Maximal mount count */
	uint16_t	s_magic;					/* Magic signature */
	uint16_t	s_state;					/* File system state */
	uint16_t	s_errors;					/* Behaviour when detecting errors */
	uint16_t	s_minor_rev_level;				/* Minor revision level */
	uint32_t	s_lastcheck;					/* Time of last check */
	uint32_t	s_checkinterval;				/* Max. time between checks */
	uint32_t	s_creator_os;					/* OS */
	uint32_t	s_rev_level;					/* Revision level */
	uint16_t	s_def_resuid;					/* Default uid for reserved blocks */
	uint16_t	s_def_resgid;					/* Default gid for reserved blocks */
	/*
	 * EXT2_DYNAMIC_REV superblocks only.
	 */
	uint32_t	s_first_ino;					/* First non-reserved inode */
	uint16_t	s_inode_size;					/* Size of inode structure */
	uint16_t	s_block_group_nr;				/* Block group # of this superblock */
	uint32_t	s_feature_compat;				/* Compatible feature set */
	uint32_t	s_feature_incompat;			 	/* Incompatible feature set */
	uint32_t	s_feature_ro_compat;				/* Readonly-compatible feature set */
	uint8_t	 	s_uuid[16];					/* 128-bit uuid for volume */
	char		s_volume_name[16];				/* Volume name */
	char		s_last_mounted[64];			 	/* Directory where last mounted */
	uint32_t	s_algorithm_usage_bitmap;			/* For compression */
	/*
	 * Performance hints.	Directory preallocation should only
	 * happen if the EXT2_COMPAT_PREALLOC flag is on.
	 */
	uint8_t		s_prealloc_blocks;				/* Nr of blocks to try to preallocate*/
	uint8_t		s_prealloc_dir_blocks;		 		/* Nr to preallocate for dirs */
	uint16_t	s_padding1;
	/*
	 * Journaling support.
	 */
	uint8_t		s_journal_uuid[16];				/* Uuid of journal superblock */
	uint32_t	s_journal_inum;					/* Inode number of journal file */
	uint32_t	s_journal_dev;					/* Device number of journal file */
	uint32_t	s_last_orphan;					/* Start of list of inodes to delete */
	uint32_t	s_hash_seed[4];					/* HTREE hash seed */
	uint8_t		s_def_hash_version;				/* Default hash version to use */
	uint8_t		s_reserved_char_pad;
	uint16_t	s_reserved_word_pad;
	uint32_t	s_default_mount_opts;
	uint32_t	s_first_meta_bg;				/* First metablock block group */
	uint32_t	s_reserved[190];				/* Padding to the end of the block */
};

/*
 * Ext2 group descriptor.
 */
struct ext2_group_desc
{
	uint32_t	bg_block_bitmap;				/* Blocks bitmap block */
	uint32_t	bg_inode_bitmap;				/* Inodes bitmap block */
	uint32_t	bg_inode_table;					/* Inodes table block */
	uint16_t	bg_free_blocks_count;		 		/* Free blocks count */
	uint16_t	bg_free_inodes_count;		 		/* Free inodes count */
	uint16_t	bg_used_dirs_count;			 	/* Directories count */
	uint16_t	bg_pad;
	uint32_t	bg_reserved[3];
};

/*
 * Ext2 on disk inode.
 */
struct ext2_inode {
	uint16_t	i_mode;						/* File mode */
	uint16_t	i_uid;						/* Low 16 bits of Owner Uid */
	uint32_t	i_size;						/* Size in bytes */
	uint32_t	i_atime;					/* Access time */
	uint32_t	i_ctime;					/* Creation time */
	uint32_t	i_mtime;					/* Modification time */
	uint32_t	i_dtime;					/* Deletion Time */
	uint16_t	i_gid;						/* Low 16 bits of Group Id */
	uint16_t	i_links_count;					/* Links count */
	uint32_t	i_blocks;					/* Blocks count */
	uint32_t	i_flags;					/* File flags */
	uint32_t	i_reserved1;
	uint32_t	i_block[EXT2_N_BLOCKS];				/* Pointers to blocks */
	uint32_t	i_generation;					/* File version (for NFS) */
	uint32_t	i_file_acl;					/* File ACL */
	uint32_t	i_dir_acl;					/* Directory ACL */
	uint32_t	i_faddr;					/* Fragment address */
	uint8_t		i_frag;						/* Fragment number */
	uint8_t		i_fsize;					/* Fragment size */
	uint16_t	i_pad1;
	uint16_t	i_uid_high;					/* High 16 bits of Owner Uid */
	uint16_t	i_gid_high;					/* High 16 bits of Group Id */
	uint32_t	i_reserved2;
};

/*
 * Ext2 directory entry.
 */
struct ext2_dir_entry {
	uint32_t	d_inode;					/* Inode number */
	uint16_t	d_rec_len;					/* Directory entry length */
	uint8_t		d_name_len;					/* Name length */
	uint8_t		d_file_type;					/* File type */
	char		d_name[EXT2_NAME_LEN];				/* File name */
};

/*
 * Ext2 in memory super block.
 */
struct ext2_sb_info {
	uint32_t			s_inodes_per_block;		/* Number of inodes per block */
	uint32_t			s_blocks_per_group;		/* Number of blocks in a group */
	uint32_t			s_inodes_per_group;		/* Number of inodes in a group */
	uint32_t			s_itb_per_group;		/* Number of inode table blocks per group */
	uint32_t			s_gdb_count;			/* Number of group descriptor blocks */
	uint32_t			s_desc_per_block;		/* Number of group descriptors per block */
	uint32_t			s_groups_count;			/* Number of groups in the fs */
	uint16_t			s_inode_size;			/* Size of inode structure */
	uint32_t			s_first_ino;			/* First non-reserved inode */
	struct buffer_head		*s_sbh;				/* Super block buffer */
	struct buffer_head **		s_group_desc;			/* Group descriptors buffers */
	struct ext2_super_block	*	s_es;				/* Pointer to the super block */
};

/* Ext2 file system operations */
extern struct inode_operations ext2_file_iops;
extern struct inode_operations ext2_fast_symlink_iops;
extern struct inode_operations ext2_page_symlink_iops;
extern struct inode_operations ext2_dir_iops;
extern struct file_operations ext2_file_fops;
extern struct file_operations ext2_dir_fops;

/* Ext2 super operations */
int init_ext2_fs();

/* Ext2 inode prototypes */
int ext2_read_inode(struct inode *inode);
int ext2_write_inode(struct inode *inode);
int ext2_put_inode(struct inode *inode);
int ext2_get_block(struct inode *inode, uint32_t block, struct buffer_head *bh_res, int create);
struct buffer_head *ext2_bread(struct inode *inode, uint32_t block, int create);

/* Ext2 inode alloc prototypes */
struct inode *ext2_new_inode(struct inode *dir, mode_t mode);
int ext2_free_inode(struct inode *inode);

/* Ext2 block alloc prototypes */
struct ext2_group_desc *ext2_get_group_desc(struct super_block *sb, uint32_t block_group, struct buffer_head **bh);
int ext2_new_block(struct inode *inode, uint32_t goal);
int ext2_free_block(struct inode *inode, uint32_t block);

/* Ext2 truncate prototypes */
void ext2_truncate(struct inode *inode);

/* Ext2 symlink prototypes */
int ext2_fast_follow_link(struct inode *dir, struct inode *inode, int flags, mode_t mode, struct inode **res_inode);
int ext2_page_follow_link(struct inode *dir, struct inode *inode, int flags, mode_t mode, struct inode **res_inode);
ssize_t ext2_fast_readlink(struct inode *inode, char *buf, size_t bufsize);
ssize_t ext2_page_readlink(struct inode *inode, char *buf, size_t bufsize);

/* Ext2 name resolution prototypes */
int ext2_lookup(struct inode *dir, const char *name, size_t name_len, struct inode **res_inode);
int ext2_create(struct inode *dir, const char *name, size_t name_len, mode_t mode, struct inode **res_inode);
int ext2_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode);
int ext2_rmdir(struct inode *dir, struct dentry *dentry);
int ext2_link(struct inode *inode, struct inode *dir, struct dentry *dentry);
int ext2_unlink(struct inode *dir, struct dentry *dentry);
int ext2_symlink(struct inode *dir, struct dentry *dentry, const char *target);
int ext2_rename(struct inode *old_dir, const char *old_name, size_t old_name_len,
		struct inode *new_dir, const char *new_name, size_t new_name_len);
int ext2_mknod(struct inode *dir, struct dentry *dentry, mode_t mode, dev_t dev);

/* Ext2 file prototypes */
int ext2_getdents64(struct file *filp, void *dirp, size_t count);

/*
 * Get Ext2 in memory super block from generic super block.
 */
static inline struct ext2_sb_info *ext2_sb(struct super_block *sb)
{
	return sb->s_fs_info;
}

/*
 * Get first block of a group descriptor.
 */
static inline uint32_t ext2_group_first_block_no(struct super_block *sb, uint32_t group_no)
{
	return group_no * ext2_sb(sb)->s_blocks_per_group + ext2_sb(sb)->s_es->s_first_data_block;
}

/*
 * Get first free bit in a bitmap block.
 */
static inline int ext2_get_free_bitmap(struct buffer_head *bh)
{
	uint32_t *bits = (uint32_t *) bh->b_data;
	register int i, j;

	for (i = 0; i < (int) bh->b_size / 4; i++)
		if (bits[i] != 0xFFFFFFFF)
			for (j = 0; j < 32; j++)
				if (!(bits[i] & (0x1 << j)))
					return 32 * i + j;

	return -1;
}

#endif
