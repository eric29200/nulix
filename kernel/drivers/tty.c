#include <drivers/serial.h>
#include <drivers/tty.h>
#include <drivers/pit.h>
#include <proc/sched.h>
#include <proc/timer.h>
#include <stderr.h>
#include <delay.h>
#include <dev.h>

#define NB_TTYS         4

/* global ttys */
static struct tty_t tty_table[NB_TTYS];
static uint32_t current_tty;
static struct timer_event_t refresh_tm;

/*
 * Lookup for a tty.
 */
static struct tty_t *tty_lookup(dev_t dev)
{
  /* asked tty */
  if (minor(dev) >= 0 && minor(dev) < NB_TTYS)
    return &tty_table[minor(dev)];

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

  while (count < n) {
    /* read next char */
    key = -1;
    if (tty->r_pos < tty->w_pos)
      key = tty->buf[tty->r_pos++];

    if (key == 0 && count == 0) {           /* ^D */
      break;
    } else if (key < 0 && count == 0) {     /* nothing to read */
      count = -EAGAIN;
      goto out;
    } else if (key < 0) {                   /* end */
      break;
    } else {
      ((unsigned char *) buf)[count] = key;
    }

    count++;
  }

out:
  return count;
}

/*
 * Write a chararcter to tty.
 */
void tty_update(char c)
{
  struct tty_t *tty;

  /* get tty */
  tty = &tty_table[current_tty];

  /* add character */
  screen_putc(&tty->screen, c);
}

/*
 * Write to TTY.
 */
size_t tty_write(dev_t dev, const void *buf, size_t n)
{
  struct tty_t *tty;
  size_t i;

  /* get tty */
  tty = tty_lookup(dev);
  if (!tty)
    return -EINVAL;

  /* put each character on the screen */
  for (i = 0; i < n; i++)
    screen_putc(&tty->screen, ((const unsigned char *) buf)[i]);

  return i;
}

/*
 * Change current tty.
 */
void tty_change(uint32_t n)
{
  if (n < NB_TTYS) {
    current_tty = n;
    tty_table[current_tty].screen.dirty = 1;
  }
}

/*
 * TTY update.
 */
static void tty_refresh()
{
  /* update current screen */
  if (tty_table[current_tty].screen.dirty)
    screen_update(&tty_table[current_tty].screen);

  /* reschedule timer */
  timer_event_mod(&refresh_tm, jiffies + ms_to_jiffies(TTY_DELAY_UPDATE_MS));
}

/*
 * Init TTYs.
 */
int init_tty()
{
  int i;

  /* init each tty */
  for (i = 0; i < NB_TTYS; i++) {
    tty_table[i].dev = DEV_TTY1 + i;
    tty_table[i].r_pos = 0;
    tty_table[i].w_pos = 0;
    tty_table[i].buf[0] = 0;
    screen_init(&tty_table[i].screen);
  }

  /* set current tty to tty0 */
  current_tty = 0;

  /* create refrsh timer */
  timer_event_init(&refresh_tm, tty_refresh, NULL, jiffies + ms_to_jiffies(TTY_DELAY_UPDATE_MS));
  timer_event_add(&refresh_tm);

  return 0;
}
