#ifndef _TTY_H_
#define _TTY_H_

#include <drivers/framebuffer.h>
#include <drivers/termios.h>

#define TTY_BUF_SIZE            1024
#define TTY_DELAY_UPDATE_MS     20

#define INTR_CHAR(tty)          ((tty)->termios.c_cc[VINTR])
#define QUIT_CHAR(tty)          ((tty)->termios.c_cc[VQUIT])
#define ERASE_CHAR(tty)         ((tty)->termios.c_cc[VERASE])
#define KILL_CHAR(tty)          ((tty)->termios.c_cc[VKILL])
#define EOF_CHAR(tty)           ((tty)->termios.c_cc[VEOF])
#define TIME_CHAR(tty)          ((tty)->termios.c_cc[VTIME])
#define MIN_CHAR(tty)           ((tty)->termios.c_cc[VMIN])
#define SWTC_CHAR(tty)          ((tty)->termios.c_cc[VSWTC])
#define START_CHAR(tty)         ((tty)->termios.c_cc[VSTART])
#define STOP_CHAR(tty)          ((tty)->termios.c_cc[VSTOP])
#define SUSP_CHAR(tty)          ((tty)->termios.c_cc[VSUSP])
#define EOL_CHAR(tty)           ((tty)->termios.c_cc[VEOL])
#define REPRINT_CHAR(tty)       ((tty)->termios.c_cc[VREPRINT])
#define DISCARD_CHAR(tty)       ((tty)->termios.c_cc[VDISCARD])
#define WERASE_CHAR(tty)        ((tty)->termios.c_cc[VWERASE])
#define LNEXT_CHAR(tty)         ((tty)->termios.c_cc[VLNEXT])
#define EOL2_CHAR(tty)          ((tty)->termios.c_cc[VEOL2])

/*
 * TTY structure.
 */
struct tty_t {
  dev_t                 dev;                  /* dev number */
  pid_t                 pgrp;                 /* process group id */
  uint32_t              w_pos;                /* write position */
  uint32_t              d_pos;                /* real buffer position (used to handle del key) */
  unsigned char         buf[TTY_BUF_SIZE];    /* tty buffer */
  struct termios_t      termios;              /* terminal i/o */
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
