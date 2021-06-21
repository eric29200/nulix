#include <drivers/serial.h>
#include <drivers/termios.h>
#include <drivers/tty.h>
#include <drivers/pit.h>
#include <proc/sched.h>
#include <proc/timer.h>
#include <ipc/signal.h>
#include <stdio.h>
#include <stderr.h>
#include <time.h>
#include <dev.h>

#define NB_TTYS         4

/* global ttys */
static struct tty_t tty_table[NB_TTYS];
static int current_tty;
static struct timer_event_t refresh_tm;

/*
 * Lookup for a tty.
 */
static struct tty_t *tty_lookup(dev_t dev)
{
  int i;

  /* current task tty */
  if (dev == DEV_TTY) {
    dev = tty_get();

    for (i = 0; i < NB_TTYS; i++)
      if (current_task->tty == tty_table[i].dev)
        return &tty_table[i];

    return NULL;
  }

  /* current active tty */
  if (dev == DEV_TTY0)
    return &tty_table[current_tty];

  /* asked tty */
  if (minor(dev) > 0 && minor(dev) <= NB_TTYS)
    return &tty_table[minor(dev) - 1];

  return NULL;
}

/*
 * Read TTY.
 */
size_t tty_read(dev_t dev, void *buf, size_t n)
{
  struct tty_t *tty;
  size_t count = 0;
  int key;

  /* get tty */
  tty = tty_lookup(dev);
  if (!tty)
    return -EINVAL;

  /* read all characters */
  while (count < n) {
    /* wait for a character */
    while (tty->r_pos >= tty->w_pos) {
      tty->r_pos = 0;
      tty->w_pos = 0;
      task_sleep(tty);
    }

    /* get key */
    key = tty->buf[tty->r_pos++];

    /* handle new key */
    switch (key) {
      case '\b':
        if(count > 0)
          ((unsigned char *) buf)[--count] = 0;
        break;
      case '\n':
        ((unsigned char *) buf)[count++] = key;
        return count;
      default:
        ((unsigned char *) buf)[count++] = key;
        break;
    }
  }

  return count;
}

/*
 * Get current task tty.
 */
dev_t tty_get()
{
  int i;

  for (i = 0; i < NB_TTYS; i++)
    if (tty_table[i].pgrp == current_task->pgid)
      return tty_table[i].dev;

  return (dev_t) -ENOENT;
}

/*
 * Write a character to tty.
 */
void tty_update(unsigned char c)
{
  struct tty_t *tty;

  /* get tty */
  tty = &tty_table[current_tty];

  /* adjust read position */
  if (tty->w_pos >= TTY_BUF_SIZE) {
    tty->r_pos = TTY_BUF_SIZE - 1;
    tty->w_pos = TTY_BUF_SIZE - 1;
  }

  /* handle special keys */
  switch (c) {
    case 13:
      c = '\n';
      break;
    case 127:
      c = '\b';
      break;
    default:
      break;
  }

  /* store character */
  tty->buf[tty->w_pos++] = c;

  /* wake up eventual process */
  task_wakeup(tty);

  /* echo character on device */
  if (L_ECHO(tty))
    tty_write(tty->dev, &c, 1);
}

/*
 * Handle escape K sequences (delete line or part of line).
 */
static void csi_K(struct tty_t *tty)
{
  uint32_t x;

  switch (tty->pars[0]) {
    case 0:                 /* erase from cursor to end of line */
      for (x = tty->fb.x; x < tty->fb.width - 1; x++)
        fb_del(&tty->fb, x, tty->fb.y);
      break;
    case 1:                 /* erase from start of line to cursor */
      tty->fb.x = 0;
      for (x = 0; x < tty->fb.x; x++)
        fb_del(&tty->fb, x, tty->fb.y);
      break;
    case 2:                 /* erase whole line */
      for (x = 0; x < tty->fb.width; x++)
        fb_del(&tty->fb, x, tty->fb.y);
      break;
    default:
      break;
  }
}

/*
 * Write to TTY.
 */
size_t tty_write(dev_t dev, const void *buf, size_t n)
{
  struct tty_t *tty;
  const char *chars;
  size_t i;

  /* get tty */
  tty = tty_lookup(dev);
  if (!tty)
    return -EINVAL;

  /* parse characters */
  chars = (const char *) buf;
  for (i = 0; i < n; i++) {
    /* start an escape sequence or write to frame buffer */
    if (tty->state == TTY_STATE_NORMAL) {
        switch (chars[i]) {
          case '\033':
            tty->state = TTY_STATE_ESCAPE;
            break;
          default:
            fb_putc(&tty->fb, chars[i]);
            break;
        }

        continue;
    }

    /* handle escape sequence */
    if (tty->state == TTY_STATE_ESCAPE) {
        switch (chars[i]) {
          case '[':
            tty->state = TTY_STATE_SQUARE;
            break;
          default:
            tty->state = TTY_STATE_NORMAL;
            break;
        }

        continue;
    }

    /* handle escape sequence */
    if (tty->state == TTY_STATE_SQUARE) {
      /* reset npars */
      for (tty->npars = 0; tty->npars < NPARS; tty->npars++)
        tty->pars[tty->npars] = 0;
      tty->npars = 0;

      tty->state = TTY_STATE_GETPARS;
      if (chars[i] == '?')
        continue;
    }

    /* get pars */
    if (tty->state == TTY_STATE_GETPARS) {
      if (chars[i] == ';' && tty->npars < NPARS - 1) {
        tty->npars++;
        continue;
      }

      if (chars[i] >= '0' && chars[i] <= '9') {
        tty->pars[tty->npars] *= 10;
        tty->pars[tty->npars] += chars[i] - '0';
        continue;
      }

      tty->state = TTY_STATE_GOTPARS;
    }

    /* handle pars */
    if (tty->state == TTY_STATE_GOTPARS) {
        tty->state = TTY_STATE_NORMAL;

        switch (chars[i]) {
          case 'H':
            if (tty->pars[0])
              tty->pars[0]--;
            if (tty->pars[1])
              tty->pars[1]--;
            fb_set_xy(&tty->fb, tty->pars[1], tty->pars[0]);
            break;
          case 'K':
            csi_K(tty);
            break;
          default:
            break;
        }

        continue;
    }
  }

  return n;
}

/*
 * Change current tty.
 */
void tty_change(uint32_t n)
{
  if (n < NB_TTYS) {
    current_tty = n;
    tty_table[current_tty].fb.dirty = 1;
  }
}

/*
 * TTY ioctl.
 */
int tty_ioctl(dev_t dev, int request, unsigned long arg)
{
  struct tty_t *tty;

  /* get tty */
  tty = tty_lookup(dev);
  if (!tty)
    return -EINVAL;

  switch (request) {
    case TCGETS:
      memcpy((struct termios_t *) arg, &tty->termios, sizeof(struct termios_t));
      break;
    case TCSETS:
      memcpy(&tty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
      break;
    case TIOCGWINSZ:
      memcpy((struct winsize_t *) arg, &tty->winsize, sizeof(struct winsize_t));
      break;
    case TIOCGPGRP:
      *((pid_t *) arg) = tty->pgrp;
      break;
    case TIOCSPGRP:
      tty->pgrp = *((pid_t *) arg);
      break;
    default:
      printf("Unknown ioctl request (%x) on device %x\n", request, dev);
      break;
  }

  return 0;
}

/*
 * Poll a tty.
 */
int tty_poll(dev_t dev)
{
  struct tty_t *tty;
  int mask = 0;

  /* get tty */
  tty = tty_lookup(dev);
  if (!tty)
    return -EINVAL;

  /* set waiting channel */
  current_task->waiting_chan = tty;

  /* check if there is some characters to read */
  if (tty->w_pos > tty->r_pos)
    mask |= POLLIN;

  return mask;
}

/*
 * Signal foreground processes group.
 */
void tty_signal_group(dev_t dev, int sig)
{
  struct tty_t *tty;

  /* get tty */
  tty = tty_lookup(dev);
  if (!tty)
    return;

  /* send signal */
  task_signal_group(tty->pgrp, sig);
}

/*
 * TTY update.
 */
static void tty_refresh()
{
  /* update current screen */
  if (tty_table[current_tty].fb.dirty)
    tty_table[current_tty].fb.update(&tty_table[current_tty].fb);

  /* reschedule timer */
  timer_event_mod(&refresh_tm, jiffies + ms_to_jiffies(TTY_DELAY_UPDATE_MS));
}

/*
 * Init TTYs.
 */
int init_tty(struct multiboot_tag_framebuffer *tag_fb)
{
  int i;

  /* init each tty */
  for (i = 0; i < NB_TTYS; i++) {
    tty_table[i].dev = DEV_TTY1 + i;
    tty_table[i].pgrp = 0;
    tty_table[i].r_pos = 0;
    tty_table[i].w_pos = 0;
    tty_table[i].buf[0] = 0;

    /* init frame buffer */
    init_framebuffer(&tty_table[i].fb, tag_fb);

    /* set winsize */
    tty_table[i].winsize.ws_row = tty_table[i].fb.height;
    tty_table[i].winsize.ws_col = tty_table[i].fb.width;
    tty_table[i].winsize.ws_xpixel = 0;
    tty_table[i].winsize.ws_ypixel = 0;

    /* init termios */
    tty_table[i].termios = (struct termios_t) {
      .c_iflag    = ICRNL,
      .c_oflag    = OPOST | ONLCR,
      .c_cflag    = 0,
      .c_lflag    = IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
      .c_line     = 0,
      .c_cc       = INIT_C_CC,
    };
  }

  /* set current tty to first tty */
  current_tty = 0;

  /* create refresh timer */
  timer_event_init(&refresh_tm, tty_refresh, NULL, jiffies + ms_to_jiffies(TTY_DELAY_UPDATE_MS));
  timer_event_add(&refresh_tm);

  return 0;
}
