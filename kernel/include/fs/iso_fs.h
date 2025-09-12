#ifndef _ISO_FS_H_
#define _ISO_FS_H_

#include <fs/fs.h>

#define ISOFS_BLOCK_SIZE_BITS		11
#define ISOFS_BLOCK_SIZE		(1 << ISOFS_BLOCK_SIZE_BITS)
#define ISOFS_VD_PRIMARY		1
#define ISOFS_MAGIC			0x9660
#define ISOFS_MAX_NAME_LEN		255

#define ISODCL(from, to)		(to - from + 1)

/*
 * ISO volume descriptor.
 */
struct iso_volume_descriptor {
	char type			[ISODCL(1,    1)];
	char id				[ISODCL(2,    6)];
	char version			[ISODCL(7,    7)];
	char data			[ISODCL(8, 2048)];
};

/*
 * ISO primary descriptor.
 */
struct iso_primary_descriptor {
	char type			[ISODCL (   1,	  1)];
	char id				[ISODCL (   2,	  6)];
	char version			[ISODCL (   7,	  7)];
	char unused1			[ISODCL (   8,	  8)];
	char system_id			[ISODCL (   9,	 40)];
	char volume_id			[ISODCL (  41,	 72)];
	char unused2			[ISODCL (  73,	 80)];
	char volume_space_size		[ISODCL (  81, 	 88)];
	char unused3			[ISODCL (  89,  120)];
	char volume_set_size		[ISODCL ( 121,  124)];
	char volume_sequence_number	[ISODCL ( 125,  128)];
	char logical_block_size		[ISODCL ( 129,  132)];
	char path_table_size		[ISODCL ( 133,  140)];
	char type_l_path_table		[ISODCL ( 141,  144)];
	char opt_type_l_path_table	[ISODCL ( 145,  148)];
	char type_m_path_table		[ISODCL ( 149,  152)];
	char opt_type_m_path_table	[ISODCL ( 153,  156)];
	char root_directory_record	[ISODCL ( 157,  190)];
	char volume_set_id		[ISODCL ( 191,  318)];
	char publisher_id		[ISODCL ( 319,  446)];
	char preparer_id		[ISODCL ( 447,  574)];
	char application_id		[ISODCL ( 575,  702)];
	char copyright_file_id		[ISODCL ( 703,  739)];
	char abstract_file_id		[ISODCL ( 740,  776)];
	char bibliographic_file_id	[ISODCL ( 777,  813)];
	char creation_date		[ISODCL ( 814,  830)];
	char modification_date		[ISODCL ( 831,  847)];
	char expiration_date		[ISODCL ( 848,  864)];
	char effective_date		[ISODCL ( 865,  881)];
	char file_structure_version	[ISODCL ( 882,  882)];
	char unused4			[ISODCL ( 883,  883)];
	char application_data		[ISODCL ( 884, 1395)];
	char unused5			[ISODCL (1396, 2048)];
};

/*
 * ISOFS root directory record.
 */
struct iso_directory_record {
	char length			[ISODCL ( 1,  1)];
	char ext_attr_length		[ISODCL ( 2,  2)];
	char extent			[ISODCL ( 3, 10)];
	char size			[ISODCL (11, 18)];
	char date			[ISODCL (19, 25)];
	char flags			[ISODCL (26, 26)];
	char file_unit_size		[ISODCL (27, 27)];
	char interleave			[ISODCL (28, 28)];
	char volume_sequence_number	[ISODCL (29, 32)];
	unsigned char name_len		[ISODCL (33, 33)];
	char name			[0];
};

/*
 * ISOFS super block in memory.
 */
struct isofs_sb_info {
	uint32_t			s_ninodes;
	uint32_t			s_nzones;
	uint32_t			s_firstdatazone;
	uint32_t			s_log_zone_size;
	uint32_t			s_max_size;
};

/*
 * Get isofs in memory super block from generic super block.
 */
static inline struct isofs_sb_info *isofs_sb(struct super_block *sb)
{
	return sb->s_fs_info;
}

/* isofs operations */
extern struct super_operations isofs_sops;

/* isofs super prototypes */
int init_iso_fs();

/* isofs inode prototypes */
int isofs_read_inode(struct inode *inode);

/* isofs name resolution prototypes */
struct dentry *isofs_lookup(struct inode *dir, struct dentry *dentry);
int isofs_get_block(struct inode *inode, uint32_t block, struct buffer_head *bh_res, int create);
struct buffer_head *isofs_bread(struct inode *inode, int block, int create);

/* isofs file operations prototypes */
int isofs_getdents64(struct file *filp, void *dirp, size_t count);

/* isofs utils prototypes */
int isofs_num711(char *p);
int isofs_num721(char *p);
int isofs_num723(char *p);
int isofs_num731(char *p);
int isofs_num733(char *p);
int isofs_date(char *p);
ino_t isofs_parent_ino(struct inode *inode);
int isofs_name_translate(char *old, size_t old_len, char *new);

/* rock ridge prototypes */
int parse_rock_ridge_inode(struct iso_directory_record *de, struct inode *inode);
int get_rock_ridge_filename(struct iso_directory_record *de, char *retname, struct inode *inode);

#endif
