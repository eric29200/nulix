#include "__stdio_impl.h"

int __to_read(FILE *fp)
{
	/* flush output buffer */
	if (fp->wpos != fp->wbase)
		fp->write(fp, 0, 0);
	fp->wpos = fp->wbase = fp->wend = 0;

	/* no reads */
	if (fp->flags & F_NORD) {
		fp->flags |= F_ERR;
		return EOF;
	}

	/* activate read buffer */
	fp->rpos = fp->rend = fp->buf + fp->buf_size;

	return (fp->flags & F_EOF) ? EOF : 0;
}
