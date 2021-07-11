#include <drivers/pty.h>
#include <drivers/tty.h>
#include <fcntl.h>
#include <stdio.h>

#define NB_PTYS     4

/* global ptys */
static struct tty_t pty_table[NB_PTYS];
static int current_pty = 1;

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
 * PTY ioctl.
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

