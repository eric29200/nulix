#ifndef _DEV_H_
#define _DEV_H_

#define DEV_TTY0		0x400		/* current active tty */
#define DEV_TTY			0x500		/* current task tty */
#define DEV_CONSOLE		0x501		/* main console */
#define DEV_PTMX		0x502		/* ptys master creator */

#define DEV_NULL		0x103		/* /dev/null device */
#define DEV_ZERO		0x105		/* /dev/zero device */
#define DEV_RANDOM		0x108		/* /dev/random device */
#define DEV_URANDOM		0x109		/* /dev/urandom device */

#define DEV_ATA_MAJOR		3		/* ata major number */
#define DEV_MOUSE_MAJOR		13		/* mouse major number */
#define DEV_FB_MAJOR		29		/* frame buffer major number */
#define DEV_PTS_MAJOR		88		/* pty major number */

#define MAX_BLKDEV		128

extern size_t *blocksize_size[MAX_BLKDEV];

#endif
