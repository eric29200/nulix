#include <stdio.h>
#include <string.h>

#include "__stdio_impl.h"

char *fgets(char *s, int size, FILE *fp)
{
	unsigned char *z;
	char *p = s;
	size_t k;
	int c;

	/* check size */
	if (size < 1)
		return NULL;
	if (size == 1) {
		*s = 0;
		return NULL;
	}

	for (size--; size;) {
		/* read in file buffer */
		if (fp->rpos != fp->rend) {
			/* read until '\n' or all buffer */
			z = memchr(fp->rpos, '\n', fp->rend - fp->rpos);
			k = z ? z - fp->rpos + 1 : fp->rend - fp->rpos;
			k = MIN(k, (size_t) size);
			memcpy(p, fp->rpos, k);

			/* update file buffer position */
			fp->rpos += k;
			p += k;
			size -= k;

			/* end */
			if (z || !size)
				break;
		}

		/* read next character */
		c = getc(fp);
		if (c < 0) {
			if (p == s || !feof(fp))
				s = 0;

			break;
		}

		/* add next character */
		size--;
		*p++ = c;
		if (c == '\n')
			break;
	}

	/* end string */
	if (s)
		*p = 0;

	return s;
}