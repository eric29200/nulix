#include <fs/fs.h>
#include <drivers/tty.h>
#include <stderr.h>
#include <dev.h>

/*
 * Read from a character device.
 */
int read_char(dev_t dev, char *buf, int count)
{
  /* TTY devices */
  if (major(dev) == major(DEV_TTY1))
    return tty_read(dev, buf, count);

  return -EINVAL;
}

/*
 * Write to a character device.
 */
int write_char(dev_t dev, const char *buf, int count)
{
  /* TTY devices */
  if (major(dev) == major(DEV_TTY1))
    return tty_write(dev, buf, count);

  return -EINVAL;
}
