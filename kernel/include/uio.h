#ifndef _UIO_H_
#define _UIO_H_

#include <stddef.h>

#define UIO_FASTIOV	8
#define UIO_MAXIOV	1024

/*
 * I/O vectors structure.
 */
struct iovec {
	void *	iov_base;
	size_t	iov_len;
};

#endif
