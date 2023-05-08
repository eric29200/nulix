#ifndef _LIBC_IOCTL_H_
#define _LIBC_IOCTL_H_

#define TIOCGWINSZ	0x5413

int ioctl(int fd, unsigned long request, ...);

#endif