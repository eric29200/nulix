#ifndef _DEV_H_
#define _DEV_H_

#define DEV_ATA0          0x300     /* ata major number */

#define DEV_TTY0          0x400     /* current active tty */
#define DEV_TTY           0x500     /* current task tty */

#define DEV_PTMX          0x502     /* pty multiplexer */

#define DEV_NULL          0x103     /* /dev/null device */
#define DEV_ZERO          0x105     /* /dev/zero device */

#define DEV_PTY_MAJOR     136       /* pseudo terminal */

#define DEV_FB_MAJOR      29        /* frame buffer */

#endif
