#ifndef _DEV_H_
#define _DEV_H_

#define DEV_TTY0		0x400		/* current active tty */
#define DEV_TTY			0x500		/* current task tty */

#define DEV_NULL		0x103		/* /dev/null device */
#define DEV_ZERO		0x105		/* /dev/zero device */
#define DEV_RANDOM		0x108		/* /dev/random device */

#define DEV_ATA_MAJOR		3		/* ata major number */
#define DEV_MOUSE_MAJOR		13		/* mouse major nubmer */

#endif
