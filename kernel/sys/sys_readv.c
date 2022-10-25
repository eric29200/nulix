#include <sys/syscall.h>
#include <stderr.h>

/*
 * Read data into multiple buffers.
 */
ssize_t sys_readv(int fd, const struct iovec_t *iov, int iovcnt)
{
	ssize_t ret = 0, n;
	int i;

	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	/* read into each buffer */
	for (i = 0; i < iovcnt; i++, iov++) {
		/* read into buffer */
		n = do_read(current_task->files->filp[fd], iov->iov_base, iov->iov_len);
		if (n < 0)
			return n;

		/* check end of file */
		ret += n;
		if (n != (ssize_t) iov->iov_len)
			break;
	}

	return ret;
}
