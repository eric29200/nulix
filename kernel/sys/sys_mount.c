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
	struct inode_t *inode;
	uint16_t magic;
	dev_t dev = 0;

	/* unused flags/data */
	UNUSED(flags);
	UNUSED(data);

	/* find file system type */
	if (type && strcmp(type, "minix") == 0)
		magic = MINIX_SUPER_MAGIC;
	else if (type && strcmp(type, "proc") == 0)
		magic = PROC_SUPER_MAGIC;
	else
		return -ENODEV;

	/* if filesystem requires dev, find it */
	if (magic == MINIX_SUPER_MAGIC) {
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

	return do_mount(magic, dev, dir_name);
}
