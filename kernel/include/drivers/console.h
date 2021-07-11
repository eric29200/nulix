#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <drivers/tty.h>

int console_write(struct tty_t *tty, const char *buf, int n);

#endif
