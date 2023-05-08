
#include <stdio.h>
#include <fcntl.h>

#include "__stdio_impl.h"

long ftell(FILE *fp)
{
	off_t pos;

	/* seek to current position */
	pos = fp->seek(fp, 0, (fp->flags & F_APP) && fp->wpos != fp->wbase ? SEEK_END : SEEK_CUR);
	if (pos < 0)
		return pos;

	/* adjust for data in buffer */
	if (fp->rend)
		pos += fp->rpos - fp->rend;
	else if (fp->wbase)
		pos += fp->wpos - fp->wbase;

	return pos;	
}
