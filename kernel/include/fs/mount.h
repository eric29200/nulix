#ifndef _MOUNT_H_
#define _MOUNT_H_

#include <fs/fs.h>

/*
 * Mounted file system structure.
 */
struct vfs_mount {
	dev_t			mnt_dev;
	char *			mnt_devname;
	char *			mnt_dirname;
	unsigned int		mnt_flags;
	struct super_block *	mnt_sb;
	struct list_head	mnt_list;
};

#endif
