#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include "__stdio_impl.h"

int fflush(FILE *fp)
{
	int ret = 0;

	/* NULL argument : flush all files */
	if (!fp) {
		for (fp = files_list; fp != NULL; fp = fp->next)
			if (fp->wpos != fp->wbase)
				ret |= fflush(fp);

		return ret;
	}

	/* flush output */
	if (fp->wpos != fp->wbase) {
		fp->write(fp, 0, 0);
		if (!fp->wpos)
			return EOF;
	}

	/* sync read position */
	if (fp->rpos != fp->rend)
		fp->seek(fp, fp->rpos - fp->rend, SEEK_CUR);

	/* clear read/write positions */
	fp->wpos = fp->wbase = fp->wend = 0;
	fp->rpos = fp->rend = 0;

	return 0;
}
