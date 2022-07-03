#ifndef _EXT2_I_H_
#define _EXT2_I_H_

#include <stddef.h>

/*
 * Ext2 in memory inode.
 */
struct ext2_inode_info_t {
	uint32_t	i_block_group;		/* Block group of this inode */
	uint32_t	i_data[15];		/* Pointers to data blocks */
	uint32_t	i_flags;		/* File flags */
	uint32_t	i_faddr;		/* Fragment address */
	uint8_t		i_frag_no;		/* Fragment number */
	uint8_t		i_frag_size;		/* Fragment size */
	uint32_t	i_file_acl;		/* File ACL */
	uint32_t	i_dir_acl;		/* Directory ACL */
	uint32_t	i_dtime;		/* Deletion time */
	uint32_t	i_generation;		/* File version (for NFS) */
};

#endif
