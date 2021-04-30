#ifndef _TTY_H_
#define _TTY_H_

#include <drivers/screen.h>

#define TTY_BUF_SIZE            256
#define TTY_DELAY_UPDATE_MS     20

/*
 * TTY structure.
 */
struct tty_t {
  dev_t dev;
  uint32_t r_pos;
  uint32_t w_pos;
  char buf[TTY_BUF_SIZE];
  struct screen_t screen;
};

int init_tty();
size_t tty_read(dev_t dev, void *buf, size_t n);
size_t tty_write(dev_t dev, const void *buf, size_t n);
void tty_update(char c);
void tty_change(uint32_t n);

#endif
