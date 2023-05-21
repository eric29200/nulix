#ifndef _LIBC_IOCTL_H_
#define _LIBC_IOCTL_H_

#define TCGETS		0x5401
#define TCSETS		0x5402
#define TIOCSPGRP	0x5410
#define TIOCGWINSZ	0x5413

int ioctl(int fd, unsigned long request, ...);

#endif