#ifndef _CONSOLE_SELECTION_H_
#define _CONSOLE_SELECTION_H_

#include <drivers/char/tty.h>

int tioclinux(struct tty *tty, unsigned long arg);

#endif
