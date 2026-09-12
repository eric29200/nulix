#ifndef _DEV_H_
#define _DEV_H_

#define DEV_UNNAMED_MAJOR	0		/* unnamed devices major number */
#define DEV_MEMORY_MAJOR	1		/* memory major number (zero, null, random...) */
#define DEV_IDE0_MAJOR		3		/* ide 0 major number */
#define DEV_TTY_MAJOR		4		/* tty major number */
#define DEV_TTYAUX_MAJOR	5		/* auxiliary tty major number */
#define DEV_LOOP_MAJOR		7		/* loop major number */
#define DEV_MISC_MAJOR		10		/* misc major number */
#define DEV_MOUSE_MAJOR		13		/* mouse major number */
#define DEV_IDE1_MAJOR		22		/* ide 1 major number */
#define DEV_FB_MAJOR		29		/* frame buffer major number */
#define DEV_IDE2_MAJOR		33		/* ide 2 major number */
#define DEV_IDE3_MAJOR		34		/* ide 3 major number */
#define DEV_IDE4_MAJOR		56		/* ide 4 major number */
#define DEV_IDE5_MAJOR		57		/* ide 5 major number */
#define DEV_PTS_MAJOR		136		/* pty major number */

#define MAX_CHRDEV		255
#define MAX_BLKDEV		255

static inline uint32_t dev_t_to_nr(dev_t dev)
{
	return (major(dev) << 8) | minor(dev);
}

#endif
