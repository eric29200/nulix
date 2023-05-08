#include <stdio.h>
#include <errno.h>

#include "__stdio_impl.h"

int fileno(FILE *fp)
{
	int fd = fp->fd;

	if (fd < 0) {
		errno = EBADF;
		return -1;
	}

	return fd;
}