#include <stdio.h>

#include "__stdio_impl.h"

int getc(FILE *fp)
{
	unsigned char c;

	/* character in buffer */
	if (fp->rpos != fp->rend)
		return *(fp)->rpos++;

	/* else read it */
	if (!__to_read(fp) && fp->read(fp, &c, 1) == 1)
		return c;

	return EOF;
}
