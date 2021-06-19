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

#define _I_FLAG(tty,f)          ((tty)->termios.c_iflag & (f))
#define _O_FLAG(tty,f)          ((tty)->termios.c_oflag & (f))
#define _C_FLAG(tty,f)          ((tty)->termios.c_cflag & (f))
#define _L_FLAG(tty,f)          ((tty)->termios.c_lflag & (f))

#define I_IGNBRK(tty)           _I_FLAG((tty),IGNBRK)
#define I_BRKINT(tty)           _I_FLAG((tty),BRKINT)
#define I_IGNPAR(tty)           _I_FLAG((tty),IGNPAR)
#define I_PARMRK(tty)           _I_FLAG((tty),PARMRK)
#define I_INPCK(tty)            _I_FLAG((tty),INPCK)
#define I_ISTRIP(tty)           _I_FLAG((tty),ISTRIP)
#define I_INLCR(tty)            _I_FLAG((tty),INLCR)
#define I_IGNCR(tty)            _I_FLAG((tty),IGNCR)
#define I_ICRNL(tty)            _I_FLAG((tty),ICRNL)
#define I_IUCLC(tty)            _I_FLAG((tty),IUCLC)
#define I_IXON(tty)             _I_FLAG((tty),IXON)
#define I_IXANY(tty)            _I_FLAG((tty),IXANY)
#define I_IXOFF(tty)            _I_FLAG((tty),IXOFF)
#define I_IMAXBEL(tty)          _I_FLAG((tty),IMAXBEL)

#define O_OPOST(tty)            _O_FLAG((tty),OPOST)
#define O_OLCUC(tty)            _O_FLAG((tty),OLCUC)
#define O_ONLCR(tty)            _O_FLAG((tty),ONLCR)
#define O_OCRNL(tty)            _O_FLAG((tty),OCRNL)
#define O_ONOCR(tty)            _O_FLAG((tty),ONOCR)
#define O_ONLRET(tty)           _O_FLAG((tty),ONLRET)
#define O_OFILL(tty)            _O_FLAG((tty),OFILL)
#define O_OFDEL(tty)            _O_FLAG((tty),OFDEL)
#define O_NLDLY(tty)            _O_FLAG((tty),NLDLY)
#define O_CRDLY(tty)            _O_FLAG((tty),CRDLY)
#define O_TABDLY(tty)           _O_FLAG((tty),TABDLY)
#define O_BSDLY(tty)            _O_FLAG((tty),BSDLY)
#define O_VTDLY(tty)            _O_FLAG((tty),VTDLY)
#define O_FFDLY(tty)            _O_FLAG((tty),FFDLY)

#define C_BAUD(tty)             _C_FLAG((tty),CBAUD)
#define C_CSIZE(tty)            _C_FLAG((tty),CSIZE)
#define C_CSTOPB(tty)           _C_FLAG((tty),CSTOPB)
#define C_CREAD(tty)            _C_FLAG((tty),CREAD)
#define C_PARENB(tty)           _C_FLAG((tty),PARENB)
#define C_PARODD(tty)           _C_FLAG((tty),PARODD)
#define C_HUPCL(tty)            _C_FLAG((tty),HUPCL)
#define C_CLOCAL(tty)           _C_FLAG((tty),CLOCAL)
#define C_CIBAUD(tty)           _C_FLAG((tty),CIBAUD)
#define C_CRTSCTS(tty)          _C_FLAG((tty),CRTSCTS)

#define L_ISIG(tty)             _L_FLAG((tty),ISIG)
#define L_ICANON(tty)           _L_FLAG((tty),ICANON)
#define L_XCASE(tty)            _L_FLAG((tty),XCASE)
#define L_ECHO(tty)             _L_FLAG((tty),ECHO)
#define L_ECHOE(tty)            _L_FLAG((tty),ECHOE)
#define L_ECHOK(tty)            _L_FLAG((tty),ECHOK)
#define L_ECHONL(tty)           _L_FLAG((tty),ECHONL)
#define L_NOFLSH(tty)           _L_FLAG((tty),NOFLSH)
#define L_TOSTOP(tty)           _L_FLAG((tty),TOSTOP)
#define L_ECHOCTL(tty)          _L_FLAG((tty),ECHOCTL)
#define L_ECHOPRT(tty)          _L_FLAG((tty),ECHOPRT)
#define L_ECHOKE(tty)           _L_FLAG((tty),ECHOKE)
#define L_FLUSHO(tty)           _L_FLAG((tty),FLUSHO)
#define L_PENDIN(tty)           _L_FLAG((tty),PENDIN)
#define L_IEXTEN(tty)           _L_FLAG((tty),IEXTEN)

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
