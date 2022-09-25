#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <drivers/tty.h>

void console_write(struct tty_t *tty);
int console_ioctl(struct tty_t *tty, int request, unsigned long arg);

#endif
