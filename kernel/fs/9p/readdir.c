#include <fs/fs.h>
#include <fs/v9fs_fs.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Read directory.
 */
int v9fs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	UNUSED(filp);
	UNUSED(dirent);
	UNUSED(filldir);
	printf("TODO: v9fs_readdir\n");
	return -EINVAL;
}