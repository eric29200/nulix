#include <fs/fs.h>
#include <drivers/tty.h>
#include <stderr.h>
#include <dev.h>

/*
 * Write to a character device.
 */
int write_char(dev_t dev, const char *buf, int count)
{
  /* TTY devices */
  if (major(dev) == 4 || major(dev) == 5)
    return tty_write(dev, buf, count);

  return -EINVAL;
}
