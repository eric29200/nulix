#include <stdio.h>

#include "__stdio_impl.h"

int putc(int _c, FILE *fp)
{
	unsigned char c = _c;

	/* put character in buffer */
	if (c != fp->lbf && fp->wpos != fp->wend)
		return *fp->wpos++ = (char) c;

	/* no way to put character */
	if (!fp->wend && __to_write(fp))
		return EOF;

	/* retry to put character in buffer */
	if (c != fp->lbf && fp->wpos != fp->wend)
		return *fp->wpos++ = c;


	/* else write it */
	if (fp->write(fp, &c, 1) == 1)
		return c;

	return EOF;
}
