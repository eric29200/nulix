#include <stdio.h>
#include <sys/uio.h>

#include "__stdio_impl.h"

size_t __stdio_write(FILE *fp, const unsigned char *buf, size_t len)
{
	struct iovec iovs[2] = {
		{ .iov_base = fp->wbase, .iov_len = fp->wpos - fp->wbase },
		{ .iov_base = (void *) buf, .iov_len = len }
	};
	struct iovec *iov = iovs;
	size_t rem = iov[0].iov_len + iov[1].iov_len;
	int iovcnt = 2;
	ssize_t cnt;

	for (;;) {
		/* write file buffer and user buffer */
		cnt = writev(fp->fd, iov, iovcnt);

		/* full write : exit */
		if (cnt == (ssize_t) rem) {
			fp->wend = fp->buf + fp->buf_size;
			fp->wpos = fp->wbase = fp->buf;
			return len;
		}

		/* handle error */
		if (cnt < 0) {
			fp->wpos = fp->wbase = fp->wend = 0;
			fp->flags |= F_ERR;
			return iovcnt == 2 ? 0 : len - iov[0].iov_len;
		}

		/* next write */
		rem -= cnt;
		if (cnt > (ssize_t) iov[0].iov_len) {
			cnt -= iov[0].iov_len;
			iov++;
			iovcnt--;
		}

		iov[0].iov_base = (unsigned char *) iov[0].iov_base + cnt;
		iov[0].iov_len -= cnt;
	}
}
