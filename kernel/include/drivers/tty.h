#ifndef _TTY_H_
#define _TTY_H_

#include <drivers/framebuffer.h>
#include <drivers/termios.h>

#define TTY_BUF_SIZE            256
#define TTY_DELAY_UPDATE_MS     20

/*
 * TTY structure.
 */
struct tty_t {
  dev_t                 dev;                  /* dev number */
  pid_t                 pgrp;                 /* process group id */
  uint32_t              r_pos;                /* read position */
  uint32_t              w_pos;                /* write positions */
  unsigned char         buf[TTY_BUF_SIZE];    /* tty buffer */
  struct winsize_t      winsize;              /* window size */
  struct framebuffer_t  fb;                   /* framebuffer of the tty */
};

int init_tty(struct multiboot_tag_framebuffer *tag_fb);
dev_t tty_get();
size_t tty_read(dev_t dev, void *buf, size_t n);
size_t tty_write(dev_t dev, const void *buf, size_t n);
void tty_update(unsigned char c);
void tty_change(uint32_t n);
int tty_ioctl(dev_t dev, int request, unsigned long arg);
void tty_signal_group(dev_t dev, int sig);

#endif
