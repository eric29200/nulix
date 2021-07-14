#ifndef _TTY_H_
#define _TTY_H_

#include <drivers/framebuffer.h>
#include <drivers/termios.h>
#include <lib/ring_buffer.h>

#define TTY_BUF_SIZE            1024
#define TTY_ESC_BUF_SIZE        16
#define TTY_DELAY_UPDATE_MS     20

#define TTY_STATE_NORMAL        0
#define TTY_STATE_ESCAPE        1
#define TTY_STATE_SQUARE        2
#define TTY_STATE_GETPARS       3
#define TTY_STATE_GOTPARS       4

#define NPARS                   16

#define _L_FLAG(tty,f)          ((tty)->termios.c_lflag & f)
#define _I_FLAG(tty,f)          ((tty)->termios.c_iflag & f)
#define _O_FLAG(tty,f)          ((tty)->termios.c_oflag & f)

#define L_CANON(tty)            _L_FLAG((tty),ICANON)
#define L_ISIG(tty)             _L_FLAG((tty),ISIG)
#define L_ECHO(tty)             _L_FLAG((tty),ECHO)
#define L_ECHOE(tty)            _L_FLAG((tty),ECHOE)
#define L_ECHOK(tty)            _L_FLAG((tty),ECHOK)
#define L_ECHOCTL(tty)          _L_FLAG((tty),ECHOCTL)
#define L_ECHOKE(tty)           _L_FLAG((tty),ECHOKE)
#define L_TOSTOP(tty)           _L_FLAG((tty),TOSTOP)

#define I_UCLC(tty)             _I_FLAG((tty),IUCLC)
#define I_NLCR(tty)             _I_FLAG((tty),INLCR)
#define I_CRNL(tty)             _I_FLAG((tty),ICRNL)
#define I_NOCR(tty)             _I_FLAG((tty),IGNCR)
#define I_IXON(tty)             _I_FLAG((tty),IXON)

#define O_POST(tty)             _O_FLAG((tty),OPOST)
#define O_NLCR(tty)             _O_FLAG((tty),ONLCR)
#define O_CRNL(tty)             _O_FLAG((tty),OCRNL)
#define O_NLRET(tty)            _O_FLAG((tty),ONLRET)
#define O_LCUC(tty)             _O_FLAG((tty),OLCUC)

/*
 * TTY structure.
 */
struct tty_t {
  dev_t                 dev;                                              /* dev number */
  pid_t                 pgrp;                                             /* process group id */
  struct ring_buffer_t  buffer;                                           /* tty ring buffer */
  uint32_t              pars[NPARS];                                      /* escaped pars */
  uint32_t              npars;                                            /* number of escaped pars */
  int                   esc_buf_size;                                     /* escape buffer size */
  int                   state;                                            /* tty state (NORMAL or ESCAPE) */
  struct winsize_t      winsize;                                          /* window size */
  struct termios_t      termios;                                          /* terminal i/o */
  struct framebuffer_t  fb;                                               /* framebuffer of the tty */
  int                   (*write)(struct tty_t *, const char *, int);      /* write function */
};

int init_tty(struct multiboot_tag_framebuffer *tag_fb);
dev_t tty_get();
void tty_update(unsigned char c);
void tty_change(uint32_t n);
void tty_signal_group(dev_t dev, int sig);

extern struct inode_operations_t tty_iops;

#endif
