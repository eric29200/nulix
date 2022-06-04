#include <sys/syscall.h>

/*
 * Write data from multiple buffers.
 */
ssize_t sys_writev(int fd, const struct iovec_t *iov, int iovcnt)
{
	ssize_t ret = 0, n;
	int i;

	/* write each buffer */
	for (i = 0; i < iovcnt; i++, iov++) {
		/* read into buffer */
		n = do_write(fd, iov->iov_base, iov->iov_len);
		if (n < 0)
			return n;

		/* check end of file */
		ret += n;
		if (n != (ssize_t) iov->iov_len)
			break;
	}

	return ret;
}
