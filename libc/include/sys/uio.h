#ifndef _LIBC_UIO_H_
#define _LIBC_UIO_H_

#include <stdio.h>

struct iovec {
	void *		iov_base;
	size_t		iov_len;
};

ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);

#endif