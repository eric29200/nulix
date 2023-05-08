#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "__stdio_impl.h"

FILE *fdopen(int fd, const char *mode)
{
	int flags;
	FILE *fp;

	/* check mode */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return NULL;
	}

	/* allocate file */
	fp = (FILE *) malloc(sizeof(FILE) + UNGET + BUFSIZ);
	if (!fp)
		return NULL;

	/* memzero file */
	memset(fp, 0, sizeof(FILE));

	/* impose mode restrictions */
	if (!strchr(mode, '+'))
		fp->flags = *mode == 'r' ? F_NOWR : F_NORD;

	/* apply close on exec flag */
	if (strchr(mode, 'e'))
		fcntl(fd, F_SETFD, FD_CLOEXEC);

	/* set append mode */
	if (*mode == 'a') {
		flags = fcntl(fd, F_GETFL);
		if (!(flags & O_APPEND))
			fcntl(fd, F_SETFL, flags | O_APPEND);

		fp->flags |= F_APP;
	}

	/* set file */
	fp->fd = fd;
	fp->buf = (unsigned char *) fp + sizeof(FILE) + UNGET;
	fp->buf_size = BUFSIZ;
	fp->read = __stdio_read;
	fp->write = __stdio_write;
	fp->seek = __stdio_seek;

	/* activiate line buffered mode for terminals */
	fp->lbf = EOF;
	if (!(fp->flags & F_NOWR) && isatty(fd))
		fp->lbf = '\n';

	/* add file to global list */
	return __file_add(fp);
}