#include <drivers/serial.h>
#include <drivers/tty.h>
#include <proc/sched.h>
#include <stderr.h>
#include <delay.h>
#include <dev.h>

#define NB_TTYS         4

/* global ttys */
static struct tty_t tty_table[NB_TTYS];
static uint32_t current_tty;

/*
 * Lookup for a tty.
 */
static struct tty_t *tty_lookup(dev_t dev)
{
  uint32_t i;

  /* current process tty */
  if (dev == DEV_TTY) {
    for (i = 0; i < NB_TTYS; i++)
      if (current_task->tty == tty_table[i].dev)
        return &tty_table[i];

    return NULL;
  }

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
  uint32_t flags;
  int key;

  /* get tty */
  tty = tty_lookup(dev);
  if (!tty)
    return -EINVAL;

  /* lock tty */
  spin_lock_irqsave(&tty->lock, flags);

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

  /* unlock tty */
out:
  spin_unlock_irqrestore(&tty->lock, flags);
  return count;
}

/*
 * Write a chararcter to tty.
 */
void tty_update(char c)
{
  struct tty_t *tty;
  uint32_t flags;

  /* get tty */
  tty = &tty_table[current_tty];

  /* add character */
  spin_lock_irqsave(&tty->lock, flags);
  screen_putc(&tty->screen, c);
  spin_unlock_irqrestore(&tty->lock, flags);
}

/*
 * Write to TTY.
 */
size_t tty_write(dev_t dev, const void *buf, size_t n)
{
  struct tty_t *tty;
  uint32_t flags;
  size_t i;

  /* get tty */
  tty = tty_lookup(dev);
  if (!tty)
    return -EINVAL;

  /* lock tty */
  spin_lock_irqsave(&tty->lock, flags);

  /* put each character on the screen */
  for (i = 0; i < n; i++)
    screen_putc(&tty->screen, ((const unsigned char *) buf)[i]);

  /* unlock tty */
  spin_unlock_irqrestore(&tty->lock, flags);

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
  for (;;) {
    msleep(TTY_DELAY_UPDATE_MS);
    screen_update(&tty_table[current_tty].screen);
  }
}

/*
 * Init TTYs.
 */
int init_tty()
{
  struct task_t *update_task;
  int i;

  /* init each tty */
  for (i = 0; i < NB_TTYS; i++) {
    tty_table[i].dev = DEV_TTY1 + i;
    tty_table[i].r_pos = 0;
    tty_table[i].w_pos = 0;
    tty_table[i].buf[0] = 0;
    screen_init(&tty_table[i].screen);
    spin_lock_init(&tty_table[i].lock);
  }

  /* set current tty to tty0 */
  current_tty = 0;

  /* create update tty task */
  update_task = create_kernel_task(tty_refresh);
  if (!update_task)
    return -ENOMEM;

  return run_task(update_task);
}
