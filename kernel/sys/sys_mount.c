#include <sys/syscall.h>
#include <fs/minix_fs.h>
#include <fs/proc_fs.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Mount system call.
 */
int sys_mount(char *dev_name, char *dir_name, char *type, unsigned long flags, void *data)
{
	struct file_system_t *fs;
	struct inode_t *inode;
	dev_t dev = 0;

	/* find file system type */
	fs = get_filesystem(type);
	if (!fs)
		return -ENODEV;

	/* if filesystem requires dev, find it */
	if (fs->requires_dev) {
		/* find device's inode */
		inode = namei(AT_FDCWD, NULL, dev_name, 1);
		if (!inode)
			return -EACCES;

		/* must be a block device */
		if (!S_ISBLK(inode->i_mode)) {
			iput(inode);
			return -ENOTBLK;
		}

		/* get device */
		dev = inode->i_rdev;
		iput(inode);
	}

	return do_mount(fs, dev, dir_name, data, flags);
}
