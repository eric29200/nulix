#include <sys/syscall.h>

/*
 * Llseek system call.
 */
int sys_llseek(int fd, uint32_t offset_high, uint32_t offset_low, off_t *result, int whence)
{
  off_t offset;
  int ret;

  /* compute offset */
  offset = (offset_high << 32) | offset_low;

  /* seek */
  *result = do_lseek(fd, offset, whence);

  return 0;
}
