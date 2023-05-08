
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "__stdio_impl.h"

int fseek(FILE *fp, long offset, int whence)
{
	/* check whence parameter */
	if (whence != SEEK_CUR && whence != SEEK_SET && whence != SEEK_END) {
		errno = -EINVAL;
		return -1;
	}

	/* adjust relative offset for unread data in buffer */
	if (whence == SEEK_CUR && fp->rend)
		offset -= fp->rend - fp->rpos;

	/* flush write bufer */
	if (fp->wpos != fp->wbase) {
		fp->write(fp, 0, 0);
		if (!fp->wpos)
			return -1;
	}

	/* leave writing mode */
	fp->wpos = fp->wbase = fp->wend = 0;

	/* seek */
	if (fp->seek(fp, offset, whence) < 0)
		return -1;

	/* discard read buffer */
	fp->rpos = fp->rend = 0;
	fp->flags &= ~F_EOF;

	return 0;
}
