#include <stdio.h>

#include "__stdio_impl.h"

int __to_write(FILE *fp)
{
	/* no writes */
	if (fp->flags & F_NOWR) {
		fp->flags |= F_ERR;
		return EOF;
	}

	/* clear read buffer */
	fp->rpos = fp->rend = 0;

	/* activate write buffer */
	fp->wpos = fp->wbase = fp->buf;
	fp->wend = fp->buf + fp->buf_size;

	return 0;
}