#ifndef _DEV_H_
#define _DEV_H_

#define DEV_MEMORY_MAJOR	1		/* memory major number (zero, null, random...) */
#define DEV_ATA_MAJOR		3		/* ata major number */
#define DEV_TTY_MAJOR		4		/* tty major number */
#define DEV_TTYAUX_MAJOR	5		/* auxiliary tty major number */
#define DEV_LOOP_MAJOR		7		/* loop major number */
#define DEV_MOUSE_MAJOR		13		/* mouse major number */
#define DEV_FB_MAJOR		29		/* frame buffer major number */
#define DEV_PTS_MAJOR		136		/* pty major number */

#define MAX_CHRDEV		255
#define MAX_BLKDEV		255

static inline uint32_t dev_t_to_nr(dev_t dev)
{
	return (major(dev) << 8) | minor(dev);
}

#endif
