#include <fs/fs.h>
#include <drivers/char/mem.h>
#include <drivers/char/tty.h>
#include <drivers/char/mouse.h>
#include <drivers/video/fb.h>
#include <dev.h>
#include <fcntl.h>

/*
 * Get character device driver.
 */
struct inode_operations *char_get_driver(struct inode *inode)
{
	/* not a character device */
	if (!inode || !S_ISCHR(inode->i_mode))
		return NULL;

	/* memory driver */
	if (major(inode->i_rdev) == DEV_MEMORY_MAJOR)
		return &memory_iops;

	/* tty driver */
	if (major(inode->i_rdev) == major(DEV_TTY0))
		return &tty_iops;

	/* console driver */
	if (inode->i_rdev == DEV_TTY || inode->i_rdev == DEV_CONSOLE)
		return &tty_iops;

	/* ptmx driver */
	if (inode->i_rdev == DEV_PTMX)
		return &ptm_iops;

	/* pty driver */
	if (major(inode->i_rdev) == DEV_PTS_MAJOR)
		return &pts_iops;

	/* mouse driver */
	if (major(inode->i_rdev) == DEV_MOUSE_MAJOR)
		return &mouse_iops;

	/* frame buffer driver */
	if (major(inode->i_rdev) == DEV_FB_MAJOR)
		return &fb_iops;

	return NULL;
}
