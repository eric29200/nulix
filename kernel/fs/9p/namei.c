#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Lookup for a file in a directory.
 */
struct dentry *v9fs_lookup(struct inode *dir, struct dentry *dentry)
{
	UNUSED(dir);
	UNUSED(dentry);
	printf("TODO: v9fs_lookup\n");
	return NULL;
}