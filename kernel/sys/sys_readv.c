#include <sys/syscall.h>

/*
 * Read data into multiple buffers.
 */
ssize_t sys_readv(int fd, const struct iovec_t *iov, int iovcnt)
{
  ssize_t ret = 0, n;
  int i;

  /* read into each buffer */
  for (i = 0; i < iovcnt; i++) {
    /* read into buffer */
    n = do_read(fd, iov->iov_base, iov->iov_len);
    if (n < 0)
      return n;

    /* check end of file */
    ret += n;
    if (n != (ssize_t) iov->iov_len)
      break;
  }

  return ret;
}
