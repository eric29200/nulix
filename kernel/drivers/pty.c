#include <drivers/pty.h>
#include <drivers/tty.h>
#include <fcntl.h>
#include <stdio.h>
#include <stderr.h>

#define NB_PTYS     4

/* global ptys */
static struct tty_t pty_table[NB_PTYS];
static int current_pty = 1;

/*
 * Lookup for a pty.
 */
static struct tty_t *pty_lookup(dev_t dev)
{
  /* asked tty */
  if (minor(dev) > 0 && minor(dev) <= NB_PTYS)
    return &pty_table[minor(dev) - 1];

  return NULL;
}

/*
 * Open ptys multiplexer = create a new pty.
 */
static int ptmx_open(struct file_t *filp)
{
  /* unused file */
  UNUSED(filp);

  /* update current pty */
  current_pty++;
  if (current_pty > NB_PTYS)
    current_pty = 1;

  return 0;
}

/*
 * PTY multiplexer ioctl.
 */
int ptmx_ioctl(struct file_t *filp, int request, unsigned long arg)
{
  UNUSED(filp);

  switch (request) {
    case TIOCGPTN:
      *((uint32_t *) arg) = current_pty;
      break;
    default:
      printf("Unknown ioctl request (%x) on device %x\n", request, DEV_PTMX);
      break;
  }

  return 0;
}

/*
 * PTY ioctl.
 */
int pty_ioctl(struct file_t *filp, int request, unsigned long arg)
{
  struct tty_t *tty;
  dev_t dev;

  /* get tty */
  dev = filp->f_inode->i_zone[0];
  tty = pty_lookup(dev);
  if (!tty)
    return -EINVAL;

  printf("ok\n");

  switch (request) {
    case TCGETS:
      memcpy((struct termios_t *) arg, &tty->termios, sizeof(struct termios_t));
      break;
    case TCSETS:
      memcpy(&tty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
      break;
    case TCSETSW:
      memcpy(&tty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
      break;
    case TCSETSF:
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
 * Init PTYs.
 */
int init_pty()
{
  int i;

  /* init each tty */
  for (i = 0; i < NB_PTYS; i++) {
    memset(&pty_table[i], 0, sizeof(struct tty_t));
    pty_table[i].dev = mkdev(DEV_PTY_MAJOR, i + 1);
    pty_table[i].pgrp = 0;
    pty_table[i].r_pos = 0;
    pty_table[i].w_pos = 0;
    pty_table[i].buf[0] = 0;
    pty_table[i].write = NULL;

    /* init termios */
    pty_table[i].termios = (struct termios_t) {
      .c_iflag    = ICRNL,
      .c_oflag    = OPOST | ONLCR,
      .c_cflag    = 0,
      .c_lflag    = IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
      .c_line     = 0,
      .c_cc       = INIT_C_CC,
    };
  }

  return 0;
}

/*
 * Pty multiplexer file operations.
 */
static struct file_operations_t ptmx_fops = {
  .open         = ptmx_open,
  .ioctl        = ptmx_ioctl,
};

/*
 * Pty multiplexer inode operations.
 */
struct inode_operations_t ptmx_iops = {
  .fops           = &ptmx_fops,
};

/*
 * Pty file operations.
 */
static struct file_operations_t pty_fops = {
  .ioctl        = pty_ioctl,
};

/*
 * Pty inode operations.
 */
struct inode_operations_t pty_iops = {
  .fops         = &pty_fops,
};
