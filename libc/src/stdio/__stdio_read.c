#include <stdio.h>
#include <unistd.h>
#include <sys/uio.h>

#include "__stdio_impl.h"

/*
 * Read to user buffer and to file buffer.
 */
size_t __stdio_read(FILE *fp, unsigned char *buf, size_t len)
{
	struct iovec iov[2] = {
		{ .iov_base = buf, .iov_len = len - !!fp->buf_size },
		{ .iov_base = fp->buf, .iov_len = fp->buf_size }
	};
	ssize_t cnt;

	/* null user buffer ? */
	if (iov[0].iov_len)
		cnt = readv(fp->fd, iov, 2);
	else
		cnt = read(fp->fd, iov[1].iov_base, iov[1].iov_len);

	/* handle error */
	if (cnt <= 0) {
		fp->flags |= cnt ? F_ERR : F_EOF;
		return 0;
	}

	/* nothing to buffer */
	if (cnt <= (ssize_t) iov[0].iov_len)
		return cnt;

	/* buffer */
	fp->rpos = fp->buf;
	fp->rend = fp->buf + cnt - iov[0].iov_len;

	/* add last character to user buffer */
	if (fp->buf_size)
		buf[len - 1] = *fp->rpos++;

	return len;
}
