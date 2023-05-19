#include <stdio.h>
#include <string.h>

#include "__stdio_impl.h"

size_t __fwritex(const unsigned char *ptr, size_t len, FILE *fp)
{
	size_t i = 0, n;

	/* can't write */
	if (!fp->wend && __to_write(fp))
		return 0;

	/* len > buffer capacity */
	if (len > (size_t) (fp->wend - fp->wpos))
		return fp->write(fp, ptr, len);

	if (fp->lbf >= 0) {
		/* Match /^(.*\n|)/ */
		for (i = len; i && ptr[i - 1] != '\n'; i--);
		if (i) {
			/* found line end */
			n = fp->write(fp, ptr, i);
			if (n < i)
				return n;

			ptr += i;
			len -= i;
		}
	}

	/* copy data to file buffer */
	memcpy(fp->wpos, ptr, len);
	fp->wpos += len;

	return len + i;
}
